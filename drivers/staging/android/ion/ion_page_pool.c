/*
 * drivers/staging/android/ion/ion_mem_pool.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/sched/signal.h>

#include "ion.h"

/*
 * We avoid atomic_long_t to minimize cache flushes at the cost of possible
 * race which would result in a small accounting inaccuracy that we can
 * tolerate.
 */
static long nr_total_pages;

static inline struct page *ion_page_pool_alloc_pages(struct ion_page_pool *pool)
{
	return alloc_pages(pool->gfp_mask, pool->order);
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
				     struct page *page)
{
	__free_pages(page, pool->order);
}

static int ion_page_pool_add(struct ion_page_pool *pool, struct page *page)
{
	spin_lock(&pool->lock);
	if (PageHighMem(page)) {
		list_add_tail(&page->lru, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&page->lru, &pool->low_items);
		pool->low_count++;
	}

	nr_total_pages += 1 << pool->order;
	mod_node_page_state(page_pgdat(page), NR_INDIRECTLY_RECLAIMABLE_BYTES,
			    (1 << (PAGE_SHIFT + pool->order)));
	spin_unlock(&pool->lock);
	return 0;
}

static struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high)
{
	struct page *page;

	if (high) {
		BUG_ON(!pool->high_count);
		page = list_first_entry(&pool->high_items, struct page, lru);
		pool->high_count--;
	} else {
		BUG_ON(!pool->low_count);
		page = list_first_entry(&pool->low_items, struct page, lru);
		pool->low_count--;
	}

	list_del(&page->lru);
	nr_total_pages -= 1 << pool->order;
	mod_node_page_state(page_pgdat(page), NR_INDIRECTLY_RECLAIMABLE_BYTES,
			    -(1 << (PAGE_SHIFT + pool->order)));
	return page;
}

struct page *ion_page_pool_alloc(struct ion_page_pool *pool, bool *from_pool)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	if (fatal_signal_pending(current))
		return ERR_PTR(-EINTR);

	if (*from_pool && spin_trylock(&pool->lock)) {
		if (pool->high_count)
			page = ion_page_pool_remove(pool, true);
		else if (pool->low_count)
			page = ion_page_pool_remove(pool, false);
		spin_unlock(&pool->lock);
	}
	if (!page) {
		page = ion_page_pool_alloc_pages(pool);
		*from_pool = false;
	}

	if (!page)
		return ERR_PTR(-ENOMEM);
	return page;
}

/*
 * Tries to allocate from only the specified Pool and returns NULL otherwise
 */
struct page *ion_page_pool_alloc_pool_only(struct ion_page_pool *pool)
{
	struct page *page = NULL;

	if (!pool)
		return ERR_PTR(-EINVAL);

	if (spin_trylock(&pool->lock)) {
		if (pool->high_count)
			page = ion_page_pool_remove(pool, true);
		else if (pool->low_count)
			page = ion_page_pool_remove(pool, false);
		spin_unlock(&pool->lock);
	}

	if (!page)
		return ERR_PTR(-ENOMEM);
	return page;
}

void ion_page_pool_free(struct ion_page_pool *pool, struct page *page)
{
	ion_page_pool_add(pool, page);
}

void ion_page_pool_free_immediate(struct ion_page_pool *pool, struct page *page)
{
	ion_page_pool_free_pages(pool, page);
}

int ion_page_pool_total(struct ion_page_pool *pool, bool high)
{
	int count = pool->low_count;

	if (high)
		count += pool->high_count;

	return count << pool->order;
}

#ifdef CONFIG_ION_SYSTEM_HEAP
long ion_page_pool_nr_pages(void)
{
	/* Correct possible overflow caused by racing writes */
	if (nr_total_pages < 0)
		nr_total_pages = 0;
	return nr_total_pages;
}
#endif

int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
			 int nr_to_scan)
{
	int freed = 0;
	bool high;

	if (current_is_kswapd())
		high = true;
	else
		high = !!(gfp_mask & __GFP_HIGHMEM);

	if (nr_to_scan == 0)
		return ion_page_pool_total(pool, high);

	while (freed < nr_to_scan) {
		struct page *page;

		spin_lock(&pool->lock);
		if (pool->low_count) {
			page = ion_page_pool_remove(pool, false);
		} else if (high && pool->high_count) {
			page = ion_page_pool_remove(pool, true);
		} else {
			spin_unlock(&pool->lock);
			break;
		}
		spin_unlock(&pool->lock);
		ion_page_pool_free_pages(pool, page);
		freed += (1 << pool->order);
	}

	return freed;
}

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order,
					   bool cached)
{
	struct ion_page_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool)
		return NULL;
	pool->high_count = 0;
	pool->low_count = 0;
	INIT_LIST_HEAD(&pool->low_items);
	INIT_LIST_HEAD(&pool->high_items);
	pool->gfp_mask = gfp_mask;
	pool->order = order;
	spin_lock_init(&pool->lock);
	plist_node_init(&pool->list, order);
	if (cached)
		pool->cached = true;

	return pool;
}

void ion_page_pool_destroy(struct ion_page_pool *pool)
{
	kfree(pool);
}
