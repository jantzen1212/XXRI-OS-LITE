# -*-makefile-*-
#
# xxri-power-menu - built from the repository's src/xxri-power-menu
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_POWER_MENU) += xxri-power-menu

#
# Paths and names
#
XXRI_POWER_MENU_VERSION	:= $(XXRI_OS_VERSION)
XXRI_POWER_MENU		:= xxri-power-menu-$(XXRI_POWER_MENU_VERSION)
XXRI_POWER_MENU_URL		:= file://$(XXRI_SRC)/xxri-power-menu
XXRI_POWER_MENU_DIR		:= $(BUILDDIR)/$(XXRI_POWER_MENU)
XXRI_POWER_MENU_BUILD_DIR	:= $(XXRI_POWER_MENU_DIR)-build
XXRI_POWER_MENU_LICENSE	:= $(XXRI_LICENSE)
XXRI_POWER_MENU_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_POWER_MENU_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_POWER_MENU_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-power-menu.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_POWER_MENU_BUILD_DIR) && mkdir -p $(XXRI_POWER_MENU_BUILD_DIR)
	@$(call world/execute, XXRI_POWER_MENU, $(CROSS_CC) $(XXRI_OS_CFLAGS) -w $(XXRI_GTK3_CFLAGS) $(XXRI_POWER_MENU_DIR)/xxri-power-menu.c -o $(XXRI_POWER_MENU_BUILD_DIR)/xxri-power-menu $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lm -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-power-menu.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-power-menu.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-power-menu)
	@$(call install_fixup, xxri-power-menu,PRIORITY,optional)
	@$(call install_fixup, xxri-power-menu,SECTION,base)
	@$(call install_fixup, xxri-power-menu,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-power-menu,DESCRIPTION,"power menu")

	@$(call install_copy, xxri-power-menu, 0, 0, 0755, $(XXRI_POWER_MENU_BUILD_DIR)/xxri-power-menu, /usr/local/bin/xxri-power-menu)
	@$(call install_tree, xxri-power-menu, 0, 0, $(XXRI_POWER_MENU_DIR)/root, /)

	@$(call install_finish, xxri-power-menu)

	@$(call touch)

# vim: syntax=make
