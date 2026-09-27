# -*-makefile-*-
#
# libfm-extra: breaks the libfm <-> menu-cache build cycle
#
PACKAGES-$(PTXCONF_XXRI_LIBFM_EXTRA) += xxri-libfm-extra

XXRI_LIBFM_EXTRA_VERSION	:= 1.3.2
XXRI_LIBFM_EXTRA_SHA256		:= a5042630304cf8e5d8cff9d565c6bd546f228b48c960153ed366a34e87cad1e5
XXRI_LIBFM_EXTRA		:= libfm-$(XXRI_LIBFM_EXTRA_VERSION)
XXRI_LIBFM_EXTRA_SUFFIX		:= tar.xz
XXRI_LIBFM_EXTRA_URL		:= https://downloads.sourceforge.net/pcmanfm/$(XXRI_LIBFM_EXTRA).$(XXRI_LIBFM_EXTRA_SUFFIX)
XXRI_LIBFM_EXTRA_SOURCE		:= $(SRCDIR)/$(XXRI_LIBFM_EXTRA).$(XXRI_LIBFM_EXTRA_SUFFIX)
XXRI_LIBFM_EXTRA_DIR		:= $(BUILDDIR)/libfm-extra-$(XXRI_LIBFM_EXTRA_VERSION)
XXRI_LIBFM_EXTRA_LICENSE	:= LGPL-2.1-or-later
XXRI_LIBFM_EXTRA_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)

XXRI_LIBFM_EXTRA_CFLAGS		:= $(XXRI_OS_CFLAGS) $(XXRI_OLD_C_FLAGS)
XXRI_LIBFM_EXTRA_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_LIBFM_EXTRA_CONF_TOOL	:= autoconf
XXRI_LIBFM_EXTRA_CONF_OPT	:= $(XXRI_AUTOCONF_LOCAL) --with-extra-only \
	--disable-gtk-doc --disable-udisks --disable-nls

$(STATEDIR)/xxri-libfm-extra.install.post:
	@$(call targetinfo)
	@$(call world/install.post, XXRI_LIBFM_EXTRA)
	$(call xxri/pkgconfig-local, XXRI_LIBFM_EXTRA, xxri-libfm-extra)
	@$(call touch)

$(STATEDIR)/xxri-libfm-extra.targetinstall:
	@$(call targetinfo)
	@$(call touch)
