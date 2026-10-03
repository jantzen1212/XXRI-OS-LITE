# -*-makefile-*-
#
# xxri-dialog - built from the repository's src/xxri-dialog
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_DIALOG) += xxri-dialog

#
# Paths and names
#
XXRI_DIALOG_VERSION	:= $(XXRI_OS_VERSION)
XXRI_DIALOG		:= xxri-dialog-$(XXRI_DIALOG_VERSION)
XXRI_DIALOG_URL		:= file://$(XXRI_SRC)/xxri-installer
XXRI_DIALOG_DIR		:= $(BUILDDIR)/$(XXRI_DIALOG)
XXRI_DIALOG_BUILD_DIR	:= $(XXRI_DIALOG_DIR)-build
XXRI_DIALOG_LICENSE	:= $(XXRI_LICENSE)
XXRI_DIALOG_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_DIALOG_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_DIALOG_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-dialog.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_DIALOG_BUILD_DIR) && mkdir -p $(XXRI_DIALOG_BUILD_DIR)
	@$(call world/execute, XXRI_DIALOG, $(CROSS_CC) $(XXRI_OS_CFLAGS) -w $(XXRI_GTK3_CFLAGS) $(XXRI_DIALOG_DIR)/xxri-dialog.c -o $(XXRI_DIALOG_BUILD_DIR)/xxri-dialog $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lm -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-dialog.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-dialog.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-dialog)
	@$(call install_fixup, xxri-dialog,PRIORITY,optional)
	@$(call install_fixup, xxri-dialog,SECTION,base)
	@$(call install_fixup, xxri-dialog,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-dialog,DESCRIPTION,"message and question dialogs")

	@$(call install_copy, xxri-dialog, 0, 0, 0755, $(XXRI_DIALOG_BUILD_DIR)/xxri-dialog, /usr/local/bin/xxri-dialog)

	@$(call install_finish, xxri-dialog)

	@$(call touch)

# vim: syntax=make
