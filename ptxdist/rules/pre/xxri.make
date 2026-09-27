# -*-makefile-*-
#
# Definitions shared by the XXRI rules.
#

# Tiny Core's libraries live in /usr/local/lib of the target sysroot (unpacked
# there by xxri-tc-sysroot); the cross linker must be told where the NEEDED
# libraries of those libraries are.
XXRI_TC_LIBDIR		:= $(PTXDIST_SYSROOT_TARGET)/usr/local/lib
XXRI_TC_LDFLAGS		:= -L$(XXRI_TC_LIBDIR) -Wl,-rpath-link,$(XXRI_TC_LIBDIR) \
	-Wl,-rpath-link,$(PTXDIST_SYSROOT_TARGET)/usr/lib -Wl,-rpath-link,$(PTXDIST_SYSROOT_TARGET)/lib
# ...and its headers: the cross compiler does not search <sysroot>/usr/local/include,
# and PTXdist's pkg-config wrapper strips it from --cflags as a "system" path
XXRI_TC_CPPFLAGS	:= -isystem $(PTXDIST_SYSROOT_TARGET)/usr/local/include

# PTXdist's compiler wrapper maps the build directory out of debug info only;
# __FILE__ (assertion and warning messages - Qt has many) would still record
# where the tree was built, so the binaries would differ between build
# directories.
XXRI_REPRO_CFLAGS	:= -fmacro-prefix-map=$(PTXDIST_PLATFORMDIR)/=

# CPU baseline of the OS programs (docs/cpu-baseline.md): the toolchain's own
# i586 default made explicit, generic tuning, size-optimised.
XXRI_OS_CFLAGS		:= -march=i586 -mtune=generic -Os $(XXRI_REPRO_CFLAGS)

# The browser engine needs SSE2 (V8), so Qt and the engine are built for the
# SSE2 instruction set; every SSE2 CPU also has CMOV.
XXRI_SSE2_CFLAGS	:= -march=pentium4 -mtune=generic -mfpmath=sse $(XXRI_REPRO_CFLAGS)

# Prefix of the browser's private Qt (packaged as xxri-browser.tcz).
XXRI_BROWSER_HOME	:= /usr/local/lib/xxri-browser

# XXRI's own programs live in the repository's src/<name>, built from there
XXRI_OS_VERSION		:= 2.6
XXRI_SRC		:= $(PTXDIST_WORKSPACE)/../src
XXRI_LICENSE		:= GPL-3.0-or-later

# pkg-config for the target: PTXdist's cross wrapper, by absolute path - a
# $(...) in a world/execute command is expanded before the PATH PTXdist sets
# for that command applies
XXRI_PKG_CONFIG		:= $(PTXDIST_SYSROOT_CROSS)/usr/bin/$(PTXCONF_GNU_TARGET)-pkg-config

# GTK 3 programs: compile and link flags from Tiny Core's gtk+-3.0
XXRI_GTK3_CFLAGS	:= $$($(XXRI_PKG_CONFIG) --cflags gtk+-3.0)
XXRI_GTK3_LIBS		:= $$($(XXRI_PKG_CONFIG) --libs gtk+-3.0)

# autotools packages XXRI installs under /usr/local, like everything Tiny
# Core adds to its base
# (static libraries are off already: PTXdist's autoconf environment sets enable_static=no)
XXRI_AUTOCONF_LOCAL	:= $(CROSS_AUTOCONF_USR) --prefix=/usr/local --libdir=/usr/local/lib \
	--sysconfdir=/etc

# PTXdist only records the pkg-config modules of /usr/{lib,share}/pkgconfig for
# the dependency whitelist; add a package's /usr/local ones ($(1): PKG, $(2): pkg)
define xxri/pkgconfig-local
	@find $($(strip $(1))_PKGDIR)/usr/local/lib/pkgconfig $($(strip $(1))_PKGDIR)/usr/local/share/pkgconfig \
		-name '*.pc' -printf '%f\n' 2>/dev/null | sed 's/\.pc$$//' >> $(STATEDIR)/$(strip $(2)).pkgconfig
endef

# GCC 14 turns these old-C constructs into errors; the 2.6 builds of libfm and
# the File fork relaxed exactly these
XXRI_OLD_C_FLAGS	:= -Wno-incompatible-pointer-types -Wno-int-conversion \
	-Wno-implicit-function-declaration -Wno-return-mismatch -Wno-discarded-qualifiers

# which packages end up where: the browser, the X server and the GRUB
# utilities are Tiny Core extensions of their own (loaded from the medium, not
# held in RAM);
# everything else is part of core.gz
XXRI_CORE_PKGS		:= xxri-base xxri-bootsplash xxri-compositor xxri-control-center \
	xxri-dialog xxri-file xxri-flwm xxri-glapi xxri-glvnd xxri-installer xxri-launcher \
	xxri-libexif xxri-libfm xxri-media xxri-menu-cache xxri-power-menu xxri-settings \
	xxri-store xxri-wctl
XXRI_BROWSER_TCZ_PKGS	:= xxri-qt5 xxri-browser
XXRI_XLIBRE_TCZ_PKGS	:= xxri-xlibre xxri-xlibre-input-libinput
XXRI_GRUB_TCZ_PKGS	:= xxri-grub-tools

# Perl modules XXRI builds for the host (host-perl-xml-parser)
XXRI_HOST_PERL5		:= $(PTXDIST_SYSROOT_HOST)/usr/lib/xxri-perl
export PERL5LIB		:= $(XXRI_HOST_PERL5)/lib/perl5

# PTXdist's own (pinned) host Python, for XXRI's image scripts - called by
# path: the python3 wrapper on PATH belongs to packages that declare it
XXRI_HOST_PYTHON	= $(PTXDIST_SYSROOT_HOST)/usr/bin/python$(PYTHON3_MAJORMINOR)
