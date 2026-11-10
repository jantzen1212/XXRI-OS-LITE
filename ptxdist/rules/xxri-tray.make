# -*-makefile-*-
#
# xxri-tray - built from the repository's src/xxri-tray
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_TRAY) += xxri-tray

#
# Paths and names
#
XXRI_TRAY_VERSION	:= $(XXRI_OS_VERSION)
XXRI_TRAY		:= xxri-tray-$(XXRI_TRAY_VERSION)
XXRI_TRAY_URL		:= file://$(XXRI_SRC)/xxri-tray
XXRI_TRAY_DIR		:= $(BUILDDIR)/$(XXRI_TRAY)
XXRI_TRAY_BUILD_DIR	:= $(XXRI_TRAY_DIR)-build
XXRI_TRAY_LICENSE	:= $(XXRI_LICENSE)
XXRI_TRAY_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_TRAY_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_TRAY_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-tray.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_TRAY_BUILD_DIR) && mkdir -p $(XXRI_TRAY_BUILD_DIR)
	@$(call world/execute, XXRI_TRAY, $(CROSS_CC) $(XXRI_OS_CFLAGS) -w $(XXRI_GTK3_CFLAGS) $(XXRI_TRAY_DIR)/xxri-tray.c -o $(XXRI_TRAY_BUILD_DIR)/xxri-tray $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lX11 -lm -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-tray.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-tray.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-tray)
	@$(call install_fixup, xxri-tray,PRIORITY,optional)
	@$(call install_fixup, xxri-tray,SECTION,base)
	@$(call install_fixup, xxri-tray,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-tray,DESCRIPTION,"notification island (system tray)")

	@$(call install_copy, xxri-tray, 0, 0, 0755, $(XXRI_TRAY_BUILD_DIR)/xxri-tray, /usr/local/bin/xxri-tray)


	@$(call install_finish, xxri-tray)

	@$(call touch)

# vim: syntax=make
