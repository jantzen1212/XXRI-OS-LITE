# -*-makefile-*-
#
# XLibre X server: Xorg (modesetting, glamor, GLX) and Xfbdev
#
PACKAGES-$(PTXCONF_XXRI_XLIBRE) += xxri-xlibre

XXRI_XLIBRE_VERSION	:= 25.1.9
XXRI_XLIBRE_SHA256	:= e453b4a8171f7fc0456bbd256c6eff514b7d8ce627032cb29f8d30e18607f9de
XXRI_XLIBRE		:= xlibre-xserver-$(XXRI_XLIBRE_VERSION)
XXRI_XLIBRE_SUFFIX	:= tar.gz
XXRI_XLIBRE_URL		:= https://github.com/X11Libre/xserver.git;tag=$(XXRI_XLIBRE)
XXRI_XLIBRE_SOURCE	:= $(SRCDIR)/$(XXRI_XLIBRE).$(XXRI_XLIBRE_SUFFIX)
XXRI_XLIBRE_DIR		:= $(BUILDDIR)/$(XXRI_XLIBRE)
XXRI_XLIBRE_LICENSE	:= MIT AND X11
XXRI_XLIBRE_LICENSE_FILES := file://COPYING;md5=b2636b4caedb65bffd833b33ac98bfcf
XXRI_XLIBRE_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_XLIBRE_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
# the server's inner loops (fb, damage, input) are CPU-bound: -O2, i586
XXRI_XLIBRE_CFLAGS	:= -march=i586 -mtune=generic -O2 $(XXRI_REPRO_CFLAGS)

# GLX loads Mesa's DRI drivers from DRI_DRIVER_PATH, which meson takes from
# `pkg-config --variable=dridriverdir dri`.  The sysroot's .pc files are
# relocatable, so without this the servers would search the build machine's
# sysroot instead of /usr/local/lib/dri.
XXRI_XLIBRE_CONF_ENV	:= PTXDIST_PKG_CONFIG_VAR_NO_SYSROOT=dridriverdir

# Paths are Tiny Core's (its xorg-server.tcz used the same), so xkbcomp,
# xkeyboard-config and the font directories of the extensions are found.
XXRI_XLIBRE_CONF_TOOL	:= meson
XXRI_XLIBRE_CONF_OPT	:= $(CROSS_MESON_USR) -Dprefix=/usr/local \
	-Dxorg=true -Dxfbdev=true -Dxephyr=false -Dxnest=false -Dxvfb=false \
	-Dxwin=false -Dxquartz=false \
	-Dkdrive_kbd=true -Dkdrive_mouse=true -Dkdrive_evdev=true -Dkdrive_tslib=false \
	-Dglamor=true -Dglx=true -Dglx_dri=true \
	-Ddri1=false -Ddri2=true -Ddri3=true -Ddrm=true \
	-Dxkb_dir=/usr/local/share/X11/xkb -Dxkb_output_dir=/var/lib/xkb \
	-Dxkb_bin_dir=/usr/local/bin \
	-Dfontrootdir=/usr/local/lib/X11/fonts \
	-Ddefault_font_path=/usr/local/lib/X11/fonts/misc,/usr/local/lib/X11/fonts/TTF,/usr/local/lib/X11/fonts/OTF,/usr/local/lib/X11/fonts/Type1,/usr/local/lib/X11/fonts/100dpi,/usr/local/lib/X11/fonts/75dpi \
	-Dlog_dir=/var/log \
	-Dsuid_wrapper=false \
	-Dpciaccess=true -Dudev=true -Dudev_kms=true -Dhal=false \
	-Dsystemd_notify=false -Dsystemd_logind=false -Dseatd_libseat=false \
	-Dint10=false -Dvgahw=false -Ddga=false -Dagp=false \
	-Dxvmc=false -Dxv=true -Dxinerama=true -Dxselinux=false -Dxcsecurity=false \
	-Dxdmcp=false -Dxdm-auth-1=false -Dlisten_tcp=false \
	-Dsha1=libcrypto -Dlibunwind=false \
	-Ddocs=false -Ddevel-docs=false -Ddocs-pdf=false -Dtests=false \
	-Dxf86-input-inputtest=false

$(STATEDIR)/xxri-xlibre.install.post:
	@$(call targetinfo)
	@$(call world/install.post, XXRI_XLIBRE)
	$(call xxri/pkgconfig-local, XXRI_XLIBRE, xxri-xlibre)
	@$(call touch)

$(STATEDIR)/xxri-xlibre.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-xlibre)
	@$(call install_fixup, xxri-xlibre,PRIORITY,optional)
	@$(call install_fixup, xxri-xlibre,SECTION,base)
	@$(call install_fixup, xxri-xlibre,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-xlibre,DESCRIPTION,"XLibre X server")

	@$(call install_copy, xxri-xlibre, 0, 0, 0755, -, /usr/local/bin/Xorg)
	@$(call install_link, xxri-xlibre, Xorg, /usr/local/bin/X)
#	# kdrive opens the console and the framebuffer itself: setuid root, as
#	# Xvesa was
	@$(call install_copy, xxri-xlibre, 0, 0, 4755, -, /usr/local/bin/Xfbdev)
	@$(call install_tree, xxri-xlibre, 0, 0, -, /usr/local/lib/xorg/modules)
	@$(call install_copy, xxri-xlibre, 0, 0, 0644, -, /usr/local/lib/xorg/protocol.txt)
#	# the quirks table only, as Tiny Core's xorg-server.tcz (XLibre also installs
#	# snippets for NVIDIA's proprietary driver, which XXRI does not ship)
	@$(call install_copy, xxri-xlibre, 0, 0, 0644, -, /usr/local/share/X11/xorg.conf.d/10-quirks.conf)
	@$(call install_copy, xxri-xlibre, 0, 0, 0755, \
		$(PTXDIST_WORKSPACE)/config/xxri-xlibre/tce.installed, /usr/local/tce.installed/xxri-xlibre)

	@$(call install_finish, xxri-xlibre)
	@$(call touch)

# vim: syntax=make
