# -*-makefile-*-
#
# xxri-launcher - built from the repository's src/xxri-launcher
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_LAUNCHER) += xxri-launcher

#
# Paths and names
#
XXRI_LAUNCHER_VERSION	:= $(XXRI_OS_VERSION)
XXRI_LAUNCHER		:= xxri-launcher-$(XXRI_LAUNCHER_VERSION)
XXRI_LAUNCHER_URL		:= file://$(XXRI_SRC)/xxri-launcher
XXRI_LAUNCHER_DIR		:= $(BUILDDIR)/$(XXRI_LAUNCHER)
XXRI_LAUNCHER_BUILD_DIR	:= $(XXRI_LAUNCHER_DIR)-build
XXRI_LAUNCHER_LICENSE	:= $(XXRI_LICENSE)
XXRI_LAUNCHER_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_LAUNCHER_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_LAUNCHER_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-launcher.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_LAUNCHER_BUILD_DIR) && mkdir -p $(XXRI_LAUNCHER_BUILD_DIR)
	@$(call world/execute, XXRI_LAUNCHER, $(CROSS_CC) $(XXRI_OS_CFLAGS) -std=gnu17 -w $(XXRI_GTK3_CFLAGS) $(XXRI_LAUNCHER_DIR)/xxri-launcher.c -o $(XXRI_LAUNCHER_BUILD_DIR)/xxri-launcher $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lX11 -lm -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-launcher.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-launcher.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-launcher)
	@$(call install_fixup, xxri-launcher,PRIORITY,optional)
	@$(call install_fixup, xxri-launcher,SECTION,base)
	@$(call install_fixup, xxri-launcher,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-launcher,DESCRIPTION,"All Apps drawer")

	@$(call install_copy, xxri-launcher, 0, 0, 0755, $(XXRI_LAUNCHER_BUILD_DIR)/xxri-launcher, /usr/local/bin/xxri-launcher)
	@$(call install_tree, xxri-launcher, 0, 0, $(XXRI_LAUNCHER_DIR)/root, /)

	@$(call install_finish, xxri-launcher)

	@$(call touch)

# vim: syntax=make
