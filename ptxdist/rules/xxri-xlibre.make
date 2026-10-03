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
# int10 (x86emu) runs the video BIOS for the vesa driver, the last X fallback.
# patches/xlibre-xserver-25.1.9/0001 (upstream 7dd2a11ae6, after 25.1.9): meson
# compared host_machine.cpu() ("i586" here) with CPU family names, so a
# 32-bit x86 build lost -D_PC - int10 never mapped the real BIOS and VBE was
# "not detected" - and lost Linux ACPI support.  (-Dint10=vm86 does not
# build in 25.1.9: its sources moved without the meson file.)
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
	-Dsuid_wrapper=true -Dlibexecdir=lib/xorg \
	-Dpciaccess=true -Dudev=true -Dudev_kms=true -Dhal=false \
	-Dsystemd_notify=false -Dsystemd_logind=false -Dseatd_libseat=false \
	-Dint10=x86emu -Dvgahw=true -Ddga=true -Dagp=false \
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

# the configuration files are packaged as they are (config/xxri-xlibre): a
# change re-runs the packaging, not the build
$(STATEDIR)/xxri-xlibre.targetinstall: $(wildcard $(PTXDIST_WORKSPACE)/config/xxri-xlibre/*)
	@$(call targetinfo)
	@$(call install_init, xxri-xlibre)
	@$(call install_fixup, xxri-xlibre,PRIORITY,optional)
	@$(call install_fixup, xxri-xlibre,SECTION,base)
	@$(call install_fixup, xxri-xlibre,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-xlibre,DESCRIPTION,"XLibre X server")

#	# Xorg runs as root, as Tiny Core's X.Org did: /usr/local/bin/Xorg is a
#	# script that runs the setuid wrapper, and Xwrapper.config tells the
#	# wrapper to keep root for any user (the session is started from the
#	# autologin shell, not from a console VT the wrapper would accept).
#	# Without logind, which Tiny Core does not have, an unprivileged Xorg
#	# cannot become DRM master or take over the VT.  Same layout as
#	# xorg-server.tcz: server and wrapper in /usr/local/lib/xorg.
	@$(call install_copy, xxri-xlibre, 0, 0, 0755, -, /usr/local/bin/Xorg)
	@$(call install_link, xxri-xlibre, Xorg, /usr/local/bin/X)
	@$(call install_copy, xxri-xlibre, 0, 0, 0755, -, /usr/local/lib/xorg/Xorg)
	@$(call install_copy, xxri-xlibre, 0, 0, 4755, -, /usr/local/lib/xorg/Xorg.wrap)
	@$(call install_copy, xxri-xlibre, 0, 0, 0644, \
		$(PTXDIST_WORKSPACE)/config/xxri-xlibre/Xwrapper.config, /usr/local/etc/X11/Xwrapper.config)
#	# the configurations ~/.xsession selects on a processor without SSE2, and
#	# on a GPU Mesa cannot accelerate safely (nouveau, NV04 .. NV4x)
	@$(foreach c, xxri-nosse2-kms.conf xxri-nosse2.conf xxri-noaccel-kms.conf, \
		$(call install_copy, xxri-xlibre, 0, 0, 0644, \
			$(PTXDIST_WORKSPACE)/config/xxri-xlibre/$(c), /usr/local/etc/X11/$(c))$(ptx/nl))
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
