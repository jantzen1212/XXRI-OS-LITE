# -*-makefile-*-
#
# libfm 1.3.2 built for GTK 3
#
PACKAGES-$(PTXCONF_XXRI_LIBFM) += xxri-libfm

XXRI_LIBFM_VERSION	:= 1.3.2
XXRI_LIBFM_SHA256	:= a5042630304cf8e5d8cff9d565c6bd546f228b48c960153ed366a34e87cad1e5
XXRI_LIBFM		:= libfm-$(XXRI_LIBFM_VERSION)
XXRI_LIBFM_SUFFIX	:= tar.xz
XXRI_LIBFM_URL		:= https://downloads.sourceforge.net/pcmanfm/$(XXRI_LIBFM).$(XXRI_LIBFM_SUFFIX)
XXRI_LIBFM_SOURCE	:= $(SRCDIR)/$(XXRI_LIBFM).$(XXRI_LIBFM_SUFFIX)
XXRI_LIBFM_DIR		:= $(BUILDDIR)/$(XXRI_LIBFM)
XXRI_LIBFM_LICENSE	:= LGPL-2.1-or-later AND GPL-2.0-or-later
XXRI_LIBFM_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)

XXRI_LIBFM_CFLAGS	:= $(XXRI_OS_CFLAGS) $(XXRI_OLD_C_FLAGS)
XXRI_LIBFM_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_LIBFM_CONF_TOOL	:= autoconf
XXRI_LIBFM_CONF_OPT	:= $(XXRI_AUTOCONF_LOCAL) --with-gtk=3 \
	--disable-gtk-doc --disable-udisks --disable-nls

XXRI_LIBFM_MODULES	:= gtk-fileprop-x-desktop gtk-fileprop-x-shortcut gtk-menu-actions \
	gtk-menu-trash vfs-menu vfs-search

$(STATEDIR)/xxri-libfm.install.post:
	@$(call targetinfo)
	@$(call world/install.post, XXRI_LIBFM)
	$(call xxri/pkgconfig-local, XXRI_LIBFM, xxri-libfm)
	@$(call touch)

$(STATEDIR)/xxri-libfm.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-libfm)
	@$(call install_fixup, xxri-libfm,PRIORITY,optional)
	@$(call install_fixup, xxri-libfm,SECTION,base)
	@$(call install_fixup, xxri-libfm,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-libfm,DESCRIPTION,"libfm file management library (GTK 3)")
	@$(foreach l, libfm libfm-gtk3 libfm-extra, \
		$(call install_lib, xxri-libfm, 0, 0, 0644, $(l), /usr/local)$(ptx/nl))
	@$(foreach m, $(XXRI_LIBFM_MODULES), \
		$(call install_copy, xxri-libfm, 0, 0, 0644, -, /usr/local/lib/libfm/modules/$(m).so)$(ptx/nl))
	@$(call install_tree, xxri-libfm, 0, 0, -, /usr/local/share/libfm)
	@$(call install_finish, xxri-libfm)
	@$(call touch)
