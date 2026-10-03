# -*-makefile-*-
#
# Qt 5.15 + Qt WebEngine for the XXRI Browser.
#
# Derived from PTXdist's own qt5 rule (rules/qt5.make, 2026.09.0), with three
# differences: only the seven modules the browser uses (from Qt's per-module
# tarballs, so the 3 GB qt-everywhere tree is never unpacked), a private
# prefix, and the browser's full engine feature set (the stock rule turns off
# WebRTC, printing/PDF, spellchecker, proprietary codecs and Pepper).
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_QT5) += xxri-qt5

#
# Paths and names
#
XXRI_QT5_VERSION	:= 5.15.17
# the name PTXdist looks up patches by: patches/qt-everywhere-opensource-src-5.15.17
XXRI_QT5		:= qt-everywhere-opensource-src-$(XXRI_QT5_VERSION)
XXRI_QT5_DIR		:= $(BUILDDIR)/$(XXRI_QT5)
XXRI_QT5_BUILD_OOT	:= YES
XXRI_QT5_LICENSE	:= (LGPL-3.0-only OR GPL-2.0-only) AND GFDL-1.3-only
XXRI_QT5_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_QT5_URL_BASE	:= https://download.qt.io/archive/qt/5.15/$(XXRI_QT5_VERSION)/submodules

# module name, sha256 (md5 from qt.io's md5sums.txt verified when pinned).
# qtlocation is here for QtPositioning (Qt WebEngine's geolocation); its map
# plugins are not built - the browser does not use QtLocation, and the bundled
# Mapbox GL code does not compile with GCC 14.
XXRI_QT5_MODULES	:= \
	qtbase:db1513cbb3f4a5bd2229f759c0839436f7fe681a800ff2bc34c4960b09e756ff \
	qtdeclarative:e1cd2d5d7e8de7b83156caae9b41f2be8b42d93414fe8d705212cda10c64d5da \
	qtwebchannel:ecfd3c1cf70c48f9e7f84016d22081bfffb56a57218230ab6a289e37d652f694 \
	qtlocation:f3e609b56730d302204e727578b2a768c7a51aff095af0c88aae89aec69414cf \
	qtsvg:750ad4cd70c0e739d2cdfb9b18ea57211fcb2af1116771131858f35956a31e6c \
	qtx11extras:eeb17b1faac1f7c20fbb81c167c9d48536111801a46b159fa4746422555bb090 \
	qtwebengine:e8edce3406939b892d301c5f4be56d64806a48664d555dbb34c93bcd5bc81186

# one PTXdist "part" per module tarball, unpacked to $(XXRI_QT5_DIR)/<module>;
# the package itself has no source of its own (the top level comes from
# config/xxri-qt5), so the parts replace the default part list
define xxri-qt5/part
XXRI_QT5_$(1)_VERSION	:= $(XXRI_QT5_VERSION)
XXRI_QT5_$(1)_SHA256	:= $(2)
XXRI_QT5_$(1)		:= $(1)-everywhere-opensource-src-$(XXRI_QT5_VERSION)
XXRI_QT5_$(1)_SUFFIX	:= tar.xz
XXRI_QT5_$(1)_URL	:= $(XXRI_QT5_URL_BASE)/$$(XXRI_QT5_$(1)).$$(XXRI_QT5_$(1)_SUFFIX)
XXRI_QT5_$(1)_SOURCE	:= $(SRCDIR)/$$(XXRI_QT5_$(1)).$$(XXRI_QT5_$(1)_SUFFIX)
XXRI_QT5_$(1)_DIR	:= $(XXRI_QT5_DIR)/$(1)
endef
$(foreach m,$(XXRI_QT5_MODULES),$(eval $(call xxri-qt5/part,$(word 1,$(subst :, ,$(m))),$(word 2,$(subst :, ,$(m))))))
XXRI_QT5_PARTS		:= $(foreach m,$(XXRI_QT5_MODULES),XXRI_QT5_$(word 1,$(subst :, ,$(m))))

XXRI_QT5_LICENSE_FILES	:= \
	file://qtbase/LICENSE.GPL2;md5=b234ee4d69f5fce4486a80fdaf4a4263 \
	file://qtbase/LICENSE.LGPL3;md5=e6a600fd5e1d9cbde2d983680233ad02 \
	file://qtbase/LICENSE.FDL;md5=6d9f2a9af4c8b8c3c769f6cc1b6aaf7e

XXRI_QT5_PREFIX		:= $(XXRI_BROWSER_HOME)

# ----------------------------------------------------------------------------
# Extract
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-qt5.extract:
	@$(call targetinfo)
	@$(call clean, $(XXRI_QT5_DIR))
	@$(call extract-all, XXRI_QT5)
	@cp $(PTXDIST_WORKSPACE)/config/xxri-qt5/configure $(PTXDIST_WORKSPACE)/config/xxri-qt5/qt.pro \
		$(XXRI_QT5_DIR)/
	@cp $(PTXDIST_WORKSPACE)/config/xxri-qt5/gitmodules $(XXRI_QT5_DIR)/.gitmodules
	@$(call patchin, XXRI_QT5, $(XXRI_QT5_DIR))
	@$(call touch)

# ----------------------------------------------------------------------------
# Prepare
# ----------------------------------------------------------------------------

XXRI_QT5_MKSPECS	 = $(call ptx/get-alternative, config/qt5, linux-ptx-g++)

# the extra section seems to confuse the JIT code (as in PTXdist's qt5 rule);
# stack protector and FORTIFY: Chromium sets its own (-fstack-protector,
# _FORTIFY_SOURCE in official builds), the platform-wide ones would clash
XXRI_QT5_WRAPPER_BLACKLIST := \
	TARGET_COMPILER_RECORD_SWITCHES \
	TARGET_HARDEN_STACK_STRONG \
	TARGET_HARDEN_FORTIFY

# the browser's instruction set, and the linker told where Tiny Core's
# libraries are (the wrapper passes these to every target compile/link)
XXRI_QT5_CFLAGS		:= $(XXRI_SSE2_CFLAGS)
XXRI_QT5_CXXFLAGS	:= $(XXRI_SSE2_CFLAGS)
XXRI_QT5_LDFLAGS	:= $(XXRI_TC_LDFLAGS)

# see PTXdist's qt5 rule: Qt disables pkg-config when cross-compiling unless
# both are set; the wrapper supplies the real search path
XXRI_QT5_PKG_CONFIG_ENV := \
	$(CROSS_ENV_PKG_CONFIG) \
	PKG_CONFIG_LIBDIR=/empty \
	PKG_CONFIG_SYSROOT_DIR=/.

XXRI_QT5_CONF_ENV := \
	$(XXRI_QT5_PKG_CONFIG_ENV) \
	MAKEFLAGS="$(PARALLELMFLAGS) --output-sync=none" \
	COMPILER_PREFIX=$(COMPILER_PREFIX) \
	PTX_QMAKE_CFLAGS="$(XXRI_SSE2_CFLAGS)"

XXRI_QT5_CONF_TOOL	:= autoconf
# --disable-webengine-embedded-build: Qt WebEngine turns its "embedded"
# (Android-like) mode on by itself when cross-compiling: overlay scrollbars,
# mobile viewport handling, no CSS hover capability, is_desktop_linux=false.
# XXRI Browser is a desktop browser.
XXRI_QT5_CONF_OPT	:= \
	-prefix $(XXRI_QT5_PREFIX) \
	-hostbindir $(XXRI_QT5_PREFIX)/host-bin \
	$(if $(filter 1,$(PTXDIST_VERBOSE)),-v) \
	-opensource \
	-confirm-license \
	-release \
	-no-feature-relocatable \
	--disable-optimized-tools \
	--disable-separate-debug-info \
	--disable-gdb-index \
	--disable-strip \
	--enable-shared \
	-xplatform linux-ptx-g++ \
	--disable-trace \
	-rpath \
	-reduce-exports \
	--enable-ltcg \
	-linker bfd \
	$(if $(filter 0,$(PTXDIST_VERBOSE)),-silent) \
	-pkg-config \
	-nomake examples \
	-nomake tests \
	-make libs \
	-make tools \
	\
	--enable-gui \
	--enable-widgets \
	-dbus-linked \
	--enable-accessibility \
	--enable-glib \
	--disable-icu \
	--enable-iconv \
	-qt-pcre \
	-system-zlib \
	--disable-journald \
	--disable-syslog \
	--enable-ssl \
	-openssl-linked \
	--disable-sctp \
	--disable-libproxy \
	--disable-cups \
	--enable-fontconfig \
	-system-freetype \
	-system-harfbuzz \
	--disable-gtk \
	-opengl desktop \
	--enable-egl \
	-qpa xcb \
	--enable-xcb \
	--enable-xcb-xlib \
	--no-bundled-xcb-xinput \
	--enable-xkbcommon \
	--disable-directfb \
	--disable-eglfs \
	--disable-gbm \
	--disable-kms \
	--disable-linuxfb \
	--disable-libudev \
	--disable-evdev \
	--disable-libinput \
	--disable-mtdev \
	--disable-tslib \
	--enable-gif \
	-system-libpng \
	-system-libjpeg \
	--disable-sql-db2 \
	--disable-sql-ibase \
	--disable-sql-mysql \
	--disable-sql-oci \
	--disable-sql-odbc \
	--disable-sql-psql \
	--disable-sql-sqlite2 \
	--disable-sql-tds \
	--disable-sql-sqlite \
	--disable-sm \
	--disable-feature-gssapi \
	--disable-vulkan \
	--disable-zstd \
	--disable-qml-debug \
	\
	--disable-webengine-embedded-build \
	--enable-webengine-alsa \
	--enable-webengine-pulseaudio \
	-qt-webengine-icu \
	-qt-webengine-ffmpeg \
	-system-webengine-opus \
	-qt-webengine-webp \
	-webengine-python-version python3 \
	--enable-webengine-pepper-plugins \
	--enable-webengine-printing-and-pdf \
	--enable-webengine-proprietary-codecs \
	--enable-webengine-spellchecker \
	--enable-webengine-webrtc \
	--enable-webengine-extensions \
	--enable-webengine-geolocation \
	--enable-webengine-webchannel \
	--disable-webengine-kerberos \
	--enable-webengine-widgets \
	--disable-build-qtpdf \
	\
	--disable-feature-geoservices_osm \
	--disable-feature-geoservices_here \
	--disable-feature-geoservices_esri \
	--disable-feature-geoservices_mapbox \
	--disable-feature-geoservices_mapboxgl \
	--disable-feature-geoservices_itemsoverlay \
	--disable-feature-location-labs-plugin

$(STATEDIR)/xxri-qt5.prepare:
	@$(call targetinfo)
	@rm -rf "$(XXRI_QT5_DIR)/qtbase/mkspecs/linux-ptx-g++"
	@mkdir "$(XXRI_QT5_DIR)/qtbase/mkspecs/linux-ptx-g++"
	@$(foreach file, $(wildcard $(XXRI_QT5_MKSPECS)/*), \
		$(XXRI_QT5_CONF_ENV) ptxd_replace_magic "$(file)" > \
		"$(XXRI_QT5_DIR)/qtbase/mkspecs/linux-ptx-g++/$(notdir $(file))"$(ptx/nl))
	@echo "Checking for 32bit g++ host compiler ..."
	@$(call world/execute, XXRI_QT5, \
		echo -e '#include <list>\n#include <errno.h>\n int main() { std::list<int> a; return 0; }' | \
		g++ -x c++  - -o /dev/null -m32 &> /dev/null || \
		ptxd_bailout "32bit g++ host compiler is missing (needed for Qt WebEngine's V8 snapshot)")
	@+$(call world/prepare, XXRI_QT5)
	@$(call touch)

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

XXRI_QT5_MAKE_ENV := \
	$(XXRI_QT5_PKG_CONFIG_ENV) \
	ICECC_REMOTE_CPP=0

# ----------------------------------------------------------------------------
# Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-qt5.install:
	@$(call targetinfo)
	@$(call world/install, XXRI_QT5)
	@find $(XXRI_QT5_PKGDIR) -name '*.qmltypes' | xargs -r rm
	@find $(XXRI_QT5_PKGDIR) -name '*.pri' -o -name '*.cmake' | \
		xargs sed -i 's;$(PTXDIST_WORKSPACE);@WORKSPACE@;g'
	@$(call touch)

# qmake for building against this Qt (the browser app): host tools from the
# cross sysroot, headers/libraries from the target sysroot, and the target's
# own prefix for rpaths
XXRI_QT5_HOSTBIN	:= $(PTXDIST_SYSROOT_CROSS)/usr/lib/xxri-qt5/bin
XXRI_QT5_QMAKE		:= $(XXRI_QT5_HOSTBIN)/qmake

$(STATEDIR)/xxri-qt5.install.post:
	@$(call targetinfo)
	@find $(XXRI_QT5_PKGDIR) -name '*.pri' -o -name '*.cmake' | \
		xargs sed -i 's;@WORKSPACE@;$(PTXDIST_WORKSPACE);g'
	@$(call world/install.post, XXRI_QT5)
	@rm -rf $(XXRI_QT5_HOSTBIN) && mkdir -p $(dir $(XXRI_QT5_HOSTBIN))
	@cp -a $(PTXDIST_SYSROOT_TARGET)$(XXRI_QT5_PREFIX)/host-bin $(XXRI_QT5_HOSTBIN)
	@( echo "[Paths]"; \
	   echo "Prefix=$(XXRI_QT5_PREFIX)"; \
	   echo "Sysroot=$(PTXDIST_SYSROOT_TARGET)"; \
	   echo "SysrootifyPrefix=true"; \
	   echo "HostPrefix=$(PTXDIST_SYSROOT_TARGET)$(XXRI_QT5_PREFIX)"; \
	   echo "HostData=$(PTXDIST_SYSROOT_TARGET)$(XXRI_QT5_PREFIX)"; \
	   echo "HostBinaries=$(XXRI_QT5_HOSTBIN)"; \
	   echo "HostLibraries=$(PTXDIST_SYSROOT_TARGET)$(XXRI_QT5_PREFIX)/lib" ) > $(XXRI_QT5_HOSTBIN)/qt.conf
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

XXRI_QT5_LIBS := Core Gui Widgets Network Qml QmlModels Quick QuickWidgets \
	WebChannel Positioning PrintSupport DBus XcbQpa Svg X11Extras \
	WebEngineCore WebEngineWidgets

# the browser's plugin set: X11 platform with its GLX/EGL integrations, the
# image formats and SVG icon engine its own UI uses (web content is decoded by
# Chromium itself); no GTK platform theme, no input-method plugins and no
# position plugins (Tiny Core has no geoclue, so geolocation requests are
# answered "unavailable" either way)
XXRI_QT5_PLUGINS := \
	platforms/libqxcb \
	xcbglintegrations/libqxcb-glx-integration \
	xcbglintegrations/libqxcb-egl-integration \
	imageformats/libqgif \
	imageformats/libqico \
	imageformats/libqjpeg \
	imageformats/libqsvg \
	iconengines/libqsvgicon

$(STATEDIR)/xxri-qt5.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-qt5)
	@$(call install_fixup, xxri-qt5,PRIORITY,optional)
	@$(call install_fixup, xxri-qt5,SECTION,base)
	@$(call install_fixup, xxri-qt5,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-qt5,DESCRIPTION,"Qt 5.15 + Qt WebEngine for the XXRI Browser")

	@$(foreach lib, $(XXRI_QT5_LIBS), \
		$(call install_copy, xxri-qt5, 0, 0, 0644, -, \
			$(XXRI_QT5_PREFIX)/lib/libQt5$(lib).so.$(XXRI_QT5_VERSION))$(ptx/nl) \
		$(call install_link, xxri-qt5, libQt5$(lib).so.$(XXRI_QT5_VERSION), \
			$(XXRI_QT5_PREFIX)/lib/libQt5$(lib).so.5)$(ptx/nl))

	@$(foreach plugin, $(XXRI_QT5_PLUGINS), \
		$(call install_copy, xxri-qt5, 0, 0, 0644, -, \
			$(XXRI_QT5_PREFIX)/plugins/$(plugin).so)$(ptx/nl))

	@$(call install_copy, xxri-qt5, 0, 0, 0755, -, \
		$(XXRI_QT5_PREFIX)/libexec/QtWebEngineProcess)
	@$(foreach f, icudtl.dat qtwebengine_resources.pak \
			qtwebengine_resources_100p.pak qtwebengine_resources_200p.pak, \
		$(call install_copy, xxri-qt5, 0, 0, 0644, -, \
			$(XXRI_QT5_PREFIX)/resources/$(f))$(ptx/nl))
	@$(call install_copy, xxri-qt5, 0, 0, 0644, -, \
		$(XXRI_QT5_PREFIX)/translations/qtwebengine_locales/en-US.pak)

	@$(call install_finish, xxri-qt5)

	@$(call touch)

# vim: syntax=make
