# -*-makefile-*-
#
# xf86-input-libinput, XLibre's fork
#
PACKAGES-$(PTXCONF_XXRI_XLIBRE_INPUT_LIBINPUT) += xxri-xlibre-input-libinput

XXRI_XLIBRE_INPUT_LIBINPUT_VERSION	:= 25.0.1
XXRI_XLIBRE_INPUT_LIBINPUT_SHA256	:= c05f045a0602b535504caeabfcc531645d91a90015662ffb88d43ee5e74e1a7f
XXRI_XLIBRE_INPUT_LIBINPUT		:= xlibre-xf86-input-libinput-$(XXRI_XLIBRE_INPUT_LIBINPUT_VERSION)
XXRI_XLIBRE_INPUT_LIBINPUT_SUFFIX	:= tar.gz
XXRI_XLIBRE_INPUT_LIBINPUT_URL		:= https://github.com/X11Libre/xf86-input-libinput.git;tag=$(XXRI_XLIBRE_INPUT_LIBINPUT)
XXRI_XLIBRE_INPUT_LIBINPUT_SOURCE	:= $(SRCDIR)/$(XXRI_XLIBRE_INPUT_LIBINPUT).$(XXRI_XLIBRE_INPUT_LIBINPUT_SUFFIX)
XXRI_XLIBRE_INPUT_LIBINPUT_DIR		:= $(BUILDDIR)/$(XXRI_XLIBRE_INPUT_LIBINPUT)
XXRI_XLIBRE_INPUT_LIBINPUT_LICENSE	:= MIT
XXRI_XLIBRE_INPUT_LIBINPUT_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_XLIBRE_INPUT_LIBINPUT_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_XLIBRE_INPUT_LIBINPUT_CFLAGS	:= $(XXRI_OS_CFLAGS)

XXRI_XLIBRE_INPUT_LIBINPUT_CONF_TOOL	:= meson
XXRI_XLIBRE_INPUT_LIBINPUT_CONF_OPT	:= $(CROSS_MESON_USR) -Dprefix=/usr/local \
	-Dxorg-module-dir=/usr/local/lib/xorg/modules/xlibre-25/drivers/input \
	-Dxorg-conf-dir=/usr/local/share/X11/xorg.conf.d \
	-Dsdkdir=/usr/local/include/xorg

$(STATEDIR)/xxri-xlibre-input-libinput.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-xlibre-input-libinput)
	@$(call install_fixup, xxri-xlibre-input-libinput,PRIORITY,optional)
	@$(call install_fixup, xxri-xlibre-input-libinput,SECTION,base)
	@$(call install_fixup, xxri-xlibre-input-libinput,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-xlibre-input-libinput,DESCRIPTION,"libinput input driver for XLibre")
	@$(call install_copy, xxri-xlibre-input-libinput, 0, 0, 0644, -, \
		/usr/local/lib/xorg/modules/xlibre-25/drivers/input/libinput_drv.so)
	@$(call install_copy, xxri-xlibre-input-libinput, 0, 0, 0644, -, \
		/usr/local/share/X11/xorg.conf.d/40-libinput.conf)
	@$(call install_finish, xxri-xlibre-input-libinput)
	@$(call touch)

# vim: syntax=make
