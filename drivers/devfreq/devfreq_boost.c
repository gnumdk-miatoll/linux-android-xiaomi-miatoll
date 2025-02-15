// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018-2019 Sultan Alsawaf <sultan@kerneltoast.com>.
 */

#define pr_fmt(fmt) "devfreq_boost: " fmt

#include <linux/devfreq_boost.h>
#include <linux/msm_drm_notify.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <uapi/linux/sched/types.h>

enum {
	SCREEN_OFF,
	INPUT_BOOST,
	MAX_BOOST
};

struct boost_dev {
	struct devfreq *df;
	struct delayed_work unboost;
	wait_queue_head_t boost_waitq;
	unsigned long boost_freq;
	unsigned long state;
};

struct df_boost_drv {
	struct boost_dev devices[DEVFREQ_MAX];
	struct notifier_block msm_drm_notif;
};

static void devfreq_unboost(struct work_struct *work);

#define BOOST_DEV_INIT(b, dev, freq) .devices[dev] = {				\
	.unboost =								\
		__DELAYED_WORK_INITIALIZER((b).devices[dev].unboost,		\
					   devfreq_unboost, 0),			\
	.boost_waitq =								\
		__WAIT_QUEUE_HEAD_INITIALIZER((b).devices[dev].boost_waitq),	\
	.boost_freq = freq							\
}

static struct df_boost_drv df_boost_drv_g __read_mostly = {
	BOOST_DEV_INIT(df_boost_drv_g, DEVFREQ_MSM_CPUBW,
		       CONFIG_DEVFREQ_MSM_CPUBW_BOOST_FREQ),
	BOOST_DEV_INIT(df_boost_drv_g, DEVFREQ_MSM_LLCCBW,
		       CONFIG_DEVFREQ_MSM_LLCCBW_BOOST_FREQ)
};

static void __devfreq_boost_kick(struct boost_dev *b, bool max)
{
	int input_boost_duration;

	if (!READ_ONCE(b->df) || test_bit(SCREEN_OFF, &b->state))
		return;

	if (max) {
		set_bit(MAX_BOOST, &b->state);
		clear_bit(INPUT_BOOST, &b->state);
		input_boost_duration = CONFIG_DEVFREQ_INPUT_BOOST_MAX_DURATION_MS;
	} else {
		set_bit(INPUT_BOOST, &b->state);
		input_boost_duration = CONFIG_DEVFREQ_INPUT_BOOST_DURATION_MS;
	}

	if (!mod_delayed_work(system_unbound_wq, &b->unboost,
		msecs_to_jiffies(input_boost_duration)))
		wake_up(&b->boost_waitq);
}

void devfreq_boost_kick(enum df_device device, bool max)
{
	struct df_boost_drv *d = &df_boost_drv_g;

	__devfreq_boost_kick(d->devices + device, max);
}

void devfreq_register_boost_device(enum df_device device, struct devfreq *df)
{
	struct df_boost_drv *d = &df_boost_drv_g;
	struct boost_dev *b;

	df->is_boost_device = true;
	b = d->devices + device;
	WRITE_ONCE(b->df, df);
}

static void devfreq_unboost(struct work_struct *work)
{
	struct boost_dev *b = container_of(to_delayed_work(work),
					   typeof(*b), unboost);

	clear_bit(MAX_BOOST, &b->state);
	clear_bit(INPUT_BOOST, &b->state);
	wake_up(&b->boost_waitq);
}

static void devfreq_update_boosts(struct boost_dev *b, unsigned long state)
{
	struct devfreq *df = b->df;

	mutex_lock(&df->lock);
	if (test_bit(SCREEN_OFF, &state)) {
		df->min_freq = df->profile->freq_table[0];
	} else if (test_bit(MAX_BOOST, &state)) {
		df->min_freq = df->max_freq;
	} else {
		df->min_freq = test_bit(INPUT_BOOST, &state) ?
			       min(b->boost_freq, df->max_freq) :
			       df->profile->freq_table[0];
	}
	update_devfreq(df);
	mutex_unlock(&df->lock);
}

static int devfreq_boost_thread(void *data)
{
	static const struct sched_param sched_max_rt_prio = {
		.sched_priority = MAX_RT_PRIO - 1
	};
	struct boost_dev *b = data;
	unsigned long old_state = 0;

	sched_setscheduler_nocheck(current, SCHED_FIFO, &sched_max_rt_prio);

	while (1) {
		bool should_stop = false;
		unsigned long curr_state;

		wait_event(b->boost_waitq,
			(curr_state = READ_ONCE(b->state)) != old_state ||
			(should_stop = kthread_should_stop()));

		if (should_stop)
			break;

		old_state = curr_state;
		devfreq_update_boosts(b, curr_state);
	}

	return 0;
}

static int msm_drm_notifier_cb(struct notifier_block *nb, unsigned long action,
			  void *data)
{
	struct df_boost_drv *d = container_of(nb, typeof(*d), msm_drm_notif);
	int i;
	struct msm_drm_notifier *evdata = data;
	int *blank = evdata->data;

	/* Parse framebuffer blank events as soon as they occur */
	if (action != MSM_DRM_EARLY_EVENT_BLANK)
		return NOTIFY_OK;

	/* Boost when the screen turns on and unboost when it turns off */
	for (i = 0; i < DEVFREQ_MAX; i++) {
		struct boost_dev *b = d->devices + i;

		if (*blank == MSM_DRM_BLANK_UNBLANK) {
			clear_bit(SCREEN_OFF, &b->state);
		} else {
			set_bit(SCREEN_OFF, &b->state);
			wake_up(&b->boost_waitq);
		}
	}

	return NOTIFY_OK;
}

static void devfreq_boost_input_event(struct input_handle *handle,
				      unsigned int type, unsigned int code,
				      int value)
{
	struct df_boost_drv *d = handle->handler->private;
	int i;

	for (i = 0; i < DEVFREQ_MAX; i++)
		__devfreq_boost_kick(d->devices + i, false);
}

static int devfreq_boost_input_connect(struct input_handler *handler,
				       struct input_dev *dev,
				       const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "devfreq_boost_handle";

	ret = input_register_handle(handle);
	if (ret)
		goto free_handle;

	ret = input_open_device(handle);
	if (ret)
		goto unregister_handle;

	return 0;

unregister_handle:
	input_unregister_handle(handle);
free_handle:
	kfree(handle);
	return ret;
}

static void devfreq_boost_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id devfreq_boost_ids[] = {
	/* Multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) }
	},
	/* Touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) }
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) }
	},
	{ }
};

static struct input_handler devfreq_boost_input_handler = {
	.event		= devfreq_boost_input_event,
	.connect	= devfreq_boost_input_connect,
	.disconnect	= devfreq_boost_input_disconnect,
	.name		= "devfreq_boost_handler",
	.id_table	= devfreq_boost_ids
};

static int __init devfreq_boost_init(void)
{
	struct df_boost_drv *d = &df_boost_drv_g;
	struct task_struct *thread[DEVFREQ_MAX];
	int i, ret;

	for (i = 0; i < DEVFREQ_MAX; i++) {
		struct boost_dev *b = d->devices + i;

		thread[i] = kthread_run_perf_critical(cpu_perf_mask,
						      devfreq_boost_thread, b,
						      "devfreq_boostd/%d", i);
		if (IS_ERR(thread[i])) {
			ret = PTR_ERR(thread[i]);
			pr_err("Failed to create kthread, err: %d\n", ret);
			goto stop_kthreads;
		}
	}

	devfreq_boost_input_handler.private = d;
	ret = input_register_handler(&devfreq_boost_input_handler);
	if (ret) {
		pr_err("Failed to register input handler, err: %d\n", ret);
		goto stop_kthreads;
	}

	d->msm_drm_notif.notifier_call = msm_drm_notifier_cb;
	d->msm_drm_notif.priority = INT_MAX;
	ret = msm_drm_register_client(&d->msm_drm_notif);
	if (ret) {
		pr_err("Failed to register fb notifier, err: %d\n", ret);
		goto unregister_handler;
	}

	return 0;

unregister_handler:
	input_unregister_handler(&devfreq_boost_input_handler);
stop_kthreads:
	while (i--)
		kthread_stop(thread[i]);
	return ret;
}
late_initcall(devfreq_boost_init);
