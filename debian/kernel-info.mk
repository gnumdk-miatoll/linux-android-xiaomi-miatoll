########################################################################
# Kernel settings
########################################################################

# Kernel variant. This is currently used only on the Source package name.
# Use 'android' for Android kernels ("downstream") or 'mainline' for upstream
# kernels.
VARIANT = android

# Kernel base version
KERNEL_BASE_VERSION = 4.14-336

# The kernel cmdline to use
KERNEL_BOOTIMAGE_CMDLINE = console=ttyMSM0,115200n8 earlycon=msm_geni_serial,0xa88000 androidboot.hardware=qcom androidboot.console=ttyMSM0 androidboot.memcg=1 lpm_levels.sleep_disabled=1 video=vfb:640x400,bpp=32,memsize=3072000 msm_rtb.filter=0x237 service_locator.enable=1 swiotlb=1 loop.max_part=7 buildvariant=user console=tty0 datapart=/dev/sda18 droidian.lvm.prefer v4l2loopback.devices=2 v4l2loopback.video_nr=62,63 v4l2loopback.card_label=Droidian_back,Droidian_front v4l2loopback.exclusive_caps=1,1 v4l2loopback.max_buffers=2 noirqdebug

# Slug for the device vendor. This is going to be used in the KERNELRELASE
# and package names.
DEVICE_VENDOR = xiaomi

# Slug for the device model. Like above.
DEVICE_MODEL = miatoll

# Marketing-friendly full-name. This will be used inside package descriptions
DEVICE_FULL_NAME = Xiaomi Miatoll

# Defconfig to use
KERNEL_DEFCONFIG = gnumdk_defconfig

# Whether to include DTBs with the image. Use 0 (no) or 1.
KERNEL_IMAGE_WITH_DTB = 1

# Path to the DTB
# If you leave this undefined, an attempt to find it automatically
# will be made.
KERNEL_IMAGE_DTB = arch/arm64/boot/dts/qcom/*.dtb

# Whether to include a DTB Overlay. Use 0 (no) or 1.
KERNEL_IMAGE_WITH_DTB_OVERLAY = 1

# Path to the DTB overlay.
# If you leave this undefined, an attempt to find it automatically
# will be made.
KERNEL_IMAGE_DTB_OVERLAY = arch/arm64/boot/dts/qcom/*.dtbo

# Whether to include the DTB Overlay into the kernel image
# Use 0 (no, default) or 1.
# dtbo.img will always be shipped in the linux-bootimage- package.
KERNEL_IMAGE_WITH_DTB_OVERLAY_IN_KERNEL = 0

# Path to a specifc configuration file for mkdtboimg.
# The default is to leave it undefined.
#KERNEL_IMAGE_DTB_OVERLAY_CONFIGURATION = debian/custom_dtbo_config.cfg

# Path to the DTB directory. Only define if KERNEL_IMAGE_DTB_OVERLAY_CONFIGURATION
# is defined too.
#KERNEL_IMAGE_DTB_OVERLAY_DTB_DIRECTORY = arch/arm64/boot/dts/qcom

# Various other settings that will be passed straight to mkbootimg
KERNEL_BOOTIMAGE_PAGE_SIZE = 4096
KERNEL_BOOTIMAGE_BASE_OFFSET = 0x00000000
KERNEL_BOOTIMAGE_KERNEL_OFFSET = 0x00008000
KERNEL_BOOTIMAGE_INITRAMFS_OFFSET = 0x01000000
KERNEL_BOOTIMAGE_SECONDIMAGE_OFFSET = 0xf00000
KERNEL_BOOTIMAGE_TAGS_OFFSET = 0x100

# Kernel bootimage version. Defaults to 0 (legacy header).
# As a rule of thumb:
# Devices launched with Android 8 and lower: version 0
# Devices launched with Android 9: version 1
# Devices launched with Android 10: version 2
# Devices launched with Android 11: version 2 or 3 (GKI)
KERNEL_BOOTIMAGE_VERSION = 2
KERNEL_BOOTIMAGE_DTB_OFFSET = 0x1f00000

########################################################################
# Android verified boot
########################################################################

# Whether to build a flashable vbmeta.img. Please note that currently
# only empty vbmeta images (disabling verified boot) can be generated.
# Use 0 (no) or 1 (default).
DEVICE_VBMETA_REQUIRED = 1

# Samsung devices require a special flag. Enable the following if your
# device is a Samsung device that requires flag 0 to be present
# Use 0 (no, default) or 1.
DEVICE_VBMETA_IS_SAMSUNG = 0

########################################################################
# Automatic flashing on package upgrades
########################################################################

# Whether to enable kernel upgrades on package upgrades. Use 0 (no) or 1.
FLASH_ENABLED = 1

# If your device is treble-ized, but aonly, you should set the following to
# 1 (yes).
FLASH_IS_AONLY = 1

# `flash-bootimage` defaults are enough for most recent devices, but legacy
# devices won't work out of the box.
# If you set the following to 1, this package will set `flash-bootimage`'s
# DEVICE_IS_AB and BOOTIMAGE_SLOT_A accordingly, thus enabling flashing
# on older devices.
#
# Do not enable if you don't know what you're doing
FLASH_IS_LEGACY_DEVICE = 0

# Device manufacturer. This must match the `ro.product.vendor.manufacturer`
# Android property. If you don't want to specify this, leave it undefined,
# FLASH_INFO_CPU will be checked instead.
FLASH_INFO_MANUFACTURER =

# Space-separated list of supported device ids as reported by fastboot
FLASH_INFO_DEVICE_IDS = joyeuse curtana excalibur gram

# Device model. This must match the `ro.product.vendor.model`
# Android property. If you don't want to specify this, leave it undefined,
# FLASH_INFO_CPU will be checked instead.
FLASH_INFO_MODEL =

# Device CPU. This will be grepped against /proc/cpuinfo to check if
# we're running on the specific device. Note this is a last-resort
# method, specifying FLASH_INFO_MANUFACTURER and FLASH_INFO_MODEL is
# recommended.
FLASH_INFO_CPU = Qualcomm Technologies, Inc SM7125

########################################################################
# Kernel build settings
########################################################################

# Whether to cross-build. Use 0 (no) or 1.
BUILD_CROSS = 1

# (Cross-build only) The build triplet to use. You'll probably want to
# use aarch64-linux-android- if building Android kernels.
BUILD_TRIPLET = aarch64-linux-android-

# (Cross-build only) The build triplet to use with clang. You'll probably
# want to use aarch64-linux-gnu- here.
BUILD_CLANG_TRIPLET = aarch64-linux-gnu-

# The compiler to use. Recent Android kernels are built with clang.
BUILD_CC = clang

# Extra paths to prepend to the PATH variable. You'll probably want
# to specify the clang path here (the default).
BUILD_PATH = /usr/lib/llvm-android-6.0-4691093/bin

# Extra packages to add to the Build-Depends section. Mainline builds
# can have this section empty, unless cross-building.
# The default is enough to install the Android toolchain, including clang.
DEB_TOOLCHAIN = linux-initramfs-halium-generic:arm64, binutils-aarch64-linux-gnu, clang-android-6.0-4691093, gcc-4.9-aarch64-linux-android, g++-4.9-aarch64-linux-android, libgcc-4.9-dev-aarch64-linux-android-cross

# Where we're building on
DEB_BUILD_ON = amd64

# Where we're going to run this kernel on
DEB_BUILD_FOR = arm64

# Target kernel architecture
KERNEL_ARCH = arm64

# Kernel target to build
KERNEL_BUILD_TARGET = Image.gz
