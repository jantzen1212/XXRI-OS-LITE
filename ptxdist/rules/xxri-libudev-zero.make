# -*-makefile-*-
#
# xxri-libudev-zero - libudev.so.1 for Store apps
#
# Tiny Core's udev 182 only has libudev.so.0, and Chromium-based AppImages
# need .so.1. Target only: nothing goes into the sysroot, so XXRI's own
# packages (XLibre's hotplug listens to udevd) keep linking udev 182.
#
PACKAGES-$(PTXCONF_XXRI_LIBUDEV_ZERO) += xxri-libudev-zero

XXRI_LIBUDEV_ZERO_VERSION	:= 1.0.3
XXRI_LIBUDEV_ZERO_SHA256	:= 0bd89b657d62d019598e6c7ed726ff8fed80e8ba092a83b484d66afb80b77da5
XXRI_LIBUDEV_ZERO		:= libudev-zero-$(XXRI_LIBUDEV_ZERO_VERSION)
XXRI_LIBUDEV_ZERO_SUFFIX	:= tar.gz
XXRI_LIBUDEV_ZERO_URL		:= https://github.com/illiliti/libudev-zero/archive/refs/tags/$(XXRI_LIBUDEV_ZERO_VERSION).$(XXRI_LIBUDEV_ZERO_SUFFIX)
XXRI_LIBUDEV_ZERO_SOURCE	:= $(SRCDIR)/$(XXRI_LIBUDEV_ZERO).$(XXRI_LIBUDEV_ZERO_SUFFIX)
XXRI_LIBUDEV_ZERO_DIR		:= $(BUILDDIR)/$(XXRI_LIBUDEV_ZERO)
XXRI_LIBUDEV_ZERO_LICENSE	:= ISC
XXRI_LIBUDEV_ZERO_LICENSE_FILES	:= file://LICENSE;md5=09eac32a84c5d70c105cbfce173a161d
XXRI_LIBUDEV_ZERO_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-libudev-zero.compile:
	@$(call targetinfo)
	@$(call world/execute, XXRI_LIBUDEV_ZERO, \
		$(MAKE) CC=$(CROSS_CC) CFLAGS="$(XXRI_OS_CFLAGS)" libudev.so.1)
	@$(call touch)

# not into the sysroot (see the header)
$(STATEDIR)/xxri-libudev-zero.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-libudev-zero.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-libudev-zero)
	@$(call install_fixup, xxri-libudev-zero,PRIORITY,optional)
	@$(call install_fixup, xxri-libudev-zero,SECTION,base)
	@$(call install_fixup, xxri-libudev-zero,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-libudev-zero,DESCRIPTION,"libudev.so.1 for downloaded apps")

	@$(call install_copy, xxri-libudev-zero, 0, 0, 0644, \
		$(XXRI_LIBUDEV_ZERO_DIR)/libudev.so.1, /usr/local/lib/libudev.so.1)

	@$(call install_finish, xxri-libudev-zero)

	@$(call touch)

# vim: syntax=make
