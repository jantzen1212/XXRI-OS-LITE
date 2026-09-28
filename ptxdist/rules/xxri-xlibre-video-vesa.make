# -*-makefile-*-
#
# xf86-video-vesa, XLibre's fork
#
PACKAGES-$(PTXCONF_XXRI_XLIBRE_VIDEO_VESA) += xxri-xlibre-video-vesa

XXRI_XLIBRE_VIDEO_VESA_VERSION	:= 25.0.0
XXRI_XLIBRE_VIDEO_VESA_SHA256	:= ac9bda772a4f759ff7fe91b571003bceb69353a3cf9e55023dad723c3f4fd5e7
XXRI_XLIBRE_VIDEO_VESA		:= xlibre-xf86-video-vesa-$(XXRI_XLIBRE_VIDEO_VESA_VERSION)
XXRI_XLIBRE_VIDEO_VESA_SUFFIX	:= tar.gz
XXRI_XLIBRE_VIDEO_VESA_URL	:= https://github.com/X11Libre/xf86-video-vesa.git;tag=$(XXRI_XLIBRE_VIDEO_VESA)
XXRI_XLIBRE_VIDEO_VESA_SOURCE	:= $(SRCDIR)/$(XXRI_XLIBRE_VIDEO_VESA).$(XXRI_XLIBRE_VIDEO_VESA_SUFFIX)
XXRI_XLIBRE_VIDEO_VESA_DIR	:= $(BUILDDIR)/$(XXRI_XLIBRE_VIDEO_VESA)
XXRI_XLIBRE_VIDEO_VESA_LICENSE	:= MIT AND X11
XXRI_XLIBRE_VIDEO_VESA_LICENSE_FILES := file://COPYING;md5=a1f0610ebdc6f314a9fa5102a8c5c1b0
XXRI_XLIBRE_VIDEO_VESA_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_XLIBRE_VIDEO_VESA_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_XLIBRE_VIDEO_VESA_CFLAGS	:= $(XXRI_OS_CFLAGS)

XXRI_XLIBRE_VIDEO_VESA_CONF_TOOL	:= autoconf
XXRI_XLIBRE_VIDEO_VESA_CONF_OPT		:= $(XXRI_AUTOCONF_LOCAL) \
	--with-xorg-module-dir=/usr/local/lib/xorg/modules/xlibre-25

# a git tag carries no configure script
$(STATEDIR)/xxri-xlibre-video-vesa.prepare:
	@$(call targetinfo)
	@cd $(XXRI_XLIBRE_VIDEO_VESA_DIR) && \
		ACLOCAL_PATH=$(PTXDIST_SYSROOT_HOST)/usr/share/aclocal:$(PTXDIST_SYSROOT_TARGET)/usr/local/share/aclocal \
		autoreconf -fi
	@$(call world/prepare, XXRI_XLIBRE_VIDEO_VESA)
	@$(call touch)

$(STATEDIR)/xxri-xlibre-video-vesa.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-xlibre-video-vesa)
	@$(call install_fixup, xxri-xlibre-video-vesa,PRIORITY,optional)
	@$(call install_fixup, xxri-xlibre-video-vesa,SECTION,base)
	@$(call install_fixup, xxri-xlibre-video-vesa,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-xlibre-video-vesa,DESCRIPTION,"VESA BIOS video driver for XLibre")
	@$(call install_copy, xxri-xlibre-video-vesa, 0, 0, 0644, -, \
		/usr/local/lib/xorg/modules/xlibre-25/drivers/vesa_drv.so)
	@$(call install_finish, xxri-xlibre-video-vesa)
	@$(call touch)

# vim: syntax=make
