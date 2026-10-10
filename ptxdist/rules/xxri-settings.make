# -*-makefile-*-
#
# xxri-settings - built from the repository's src/xxri-settings
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_SETTINGS) += xxri-settings

#
# Paths and names
#
XXRI_SETTINGS_VERSION	:= $(XXRI_OS_VERSION)
XXRI_SETTINGS		:= xxri-settings-$(XXRI_SETTINGS_VERSION)
XXRI_SETTINGS_URL		:= file://$(XXRI_SRC)/xxri-settings
XXRI_SETTINGS_DIR		:= $(BUILDDIR)/$(XXRI_SETTINGS)
XXRI_SETTINGS_BUILD_DIR	:= $(XXRI_SETTINGS_DIR)-build
XXRI_SETTINGS_LICENSE	:= $(XXRI_LICENSE)
XXRI_SETTINGS_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_SETTINGS_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_SETTINGS_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-settings.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_SETTINGS_BUILD_DIR) && mkdir -p $(XXRI_SETTINGS_BUILD_DIR)
	@$(call world/execute, XXRI_SETTINGS, $(CROSS_CXX) $(XXRI_OS_CFLAGS) -w -fpermissive $(XXRI_GTK3_CFLAGS) $(XXRI_SETTINGS_DIR)/xxri-settings.c -o $(XXRI_SETTINGS_BUILD_DIR)/xxri-settings $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -mtls-dialect=gnu -static-libgcc)
	@$(call xxri/mo-compile, XXRI_SETTINGS, xxri-settings)
	@$(call touch)

$(STATEDIR)/xxri-settings.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-settings.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-settings)
	@$(call install_fixup, xxri-settings,PRIORITY,optional)
	@$(call install_fixup, xxri-settings,SECTION,base)
	@$(call install_fixup, xxri-settings,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-settings,DESCRIPTION,"Settings")

	@$(call install_copy, xxri-settings, 0, 0, 0755, $(XXRI_SETTINGS_BUILD_DIR)/xxri-settings, /usr/local/bin/xxri-settings)
	@$(call install_tree, xxri-settings, 0, 0, $(XXRI_SETTINGS_DIR)/root, /)

	@$(call xxri/mo-install, xxri-settings, XXRI_SETTINGS)

	@$(call install_finish, xxri-settings)

	@$(call touch)

# vim: syntax=make
