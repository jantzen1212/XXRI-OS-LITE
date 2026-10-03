# -*-makefile-*-
#
# xxri-installer - built from the repository's src/xxri-installer
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_INSTALLER) += xxri-installer

#
# Paths and names
#
XXRI_INSTALLER_VERSION	:= $(XXRI_OS_VERSION)
XXRI_INSTALLER		:= xxri-installer-$(XXRI_INSTALLER_VERSION)
XXRI_INSTALLER_URL		:= file://$(XXRI_SRC)/xxri-installer
XXRI_INSTALLER_DIR		:= $(BUILDDIR)/$(XXRI_INSTALLER)
XXRI_INSTALLER_BUILD_DIR	:= $(XXRI_INSTALLER_DIR)-build
XXRI_INSTALLER_LICENSE	:= $(XXRI_LICENSE)
XXRI_INSTALLER_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_INSTALLER_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_INSTALLER_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-installer.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_INSTALLER_BUILD_DIR) && mkdir -p $(XXRI_INSTALLER_BUILD_DIR)
	@$(call world/execute, XXRI_INSTALLER, $(CROSS_CC) $(XXRI_OS_CFLAGS) -w $(XXRI_GTK3_CFLAGS) $(XXRI_INSTALLER_DIR)/xxri-installer.c -o $(XXRI_INSTALLER_BUILD_DIR)/xxri-installer $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lX11 -lm -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-installer.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

# the scripts under root/ are packaged as they are: an edited one re-runs
# the packaging (not the compile)
$(STATEDIR)/xxri-installer.targetinstall: $(shell find $(XXRI_SRC)/xxri-installer/root -type f)
	@$(call targetinfo)

	@$(call install_init, xxri-installer)
	@$(call install_fixup, xxri-installer,PRIORITY,optional)
	@$(call install_fixup, xxri-installer,SECTION,base)
	@$(call install_fixup, xxri-installer,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-installer,DESCRIPTION,"the disk installer")

	@$(call install_copy, xxri-installer, 0, 0, 0755, $(XXRI_INSTALLER_BUILD_DIR)/xxri-installer, /usr/local/bin/xxri-installer)
	@$(call install_tree, xxri-installer, 0, 0, $(XXRI_INSTALLER_DIR)/root, /)

	@$(call install_finish, xxri-installer)

	@$(call touch)

# vim: syntax=make
