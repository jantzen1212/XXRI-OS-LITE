# -*-makefile-*-
#
# xxri-locales - UTF-8 locales, compiled with the toolchain's localedef (the
# same glibc as Tiny Core's base)
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_LOCALES) += xxri-locales

#
# Paths and names
#
XXRI_LOCALES_VERSION	:= $(XXRI_OS_VERSION)
XXRI_LOCALES		:= xxri-locales-$(XXRI_LOCALES_VERSION)
XXRI_LOCALES_DIR	:= $(BUILDDIR)/$(XXRI_LOCALES)
XXRI_LOCALES_LICENSE	:= LGPL-2.1-or-later

# glibc locale names; <name> becomes /usr/lib/locale/<name>.utf8, used with
# LANG=<name>.UTF-8.  The sources are in the toolchain's usr/share/i18n/locales.
XXRI_LOCALES_LIST	:= en_US en_GB id_ID pt_PT

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-locales.extract $(STATEDIR)/xxri-locales.prepare:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-locales.compile: $(PTXDIST_WORKSPACE)/scripts/xxri-locales.sh
	@$(call targetinfo)
	@$(PTXDIST_WORKSPACE)/scripts/xxri-locales.sh "$$($(CROSS_CC) -print-sysroot)" \
		$(XXRI_LOCALES_DIR) $(XXRI_LOCALES_LIST)
	@$(call touch)

$(STATEDIR)/xxri-locales.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-locales.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-locales)
	@$(call install_fixup, xxri-locales,PRIORITY,optional)
	@$(call install_fixup, xxri-locales,SECTION,base)
	@$(call install_fixup, xxri-locales,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-locales,DESCRIPTION,"UTF-8 locales")

	@$(call install_tree, xxri-locales, 0, 0, $(XXRI_LOCALES_DIR), /usr/lib/locale)

	@$(call install_finish, xxri-locales)

	@$(call touch)

# vim: syntax=make
