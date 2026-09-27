# -*-makefile-*-
#
# xxri-control-center - built from the repository's src/xxri-control-center
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_CONTROL_CENTER) += xxri-control-center

#
# Paths and names
#
XXRI_CONTROL_CENTER_VERSION	:= $(XXRI_OS_VERSION)
XXRI_CONTROL_CENTER		:= xxri-control-center-$(XXRI_CONTROL_CENTER_VERSION)
XXRI_CONTROL_CENTER_URL		:= file://$(XXRI_SRC)/xxri-control-center
XXRI_CONTROL_CENTER_DIR		:= $(BUILDDIR)/$(XXRI_CONTROL_CENTER)
XXRI_CONTROL_CENTER_BUILD_DIR	:= $(XXRI_CONTROL_CENTER_DIR)-build
XXRI_CONTROL_CENTER_LICENSE	:= $(XXRI_LICENSE)
XXRI_CONTROL_CENTER_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_CONTROL_CENTER_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_CONTROL_CENTER_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-control-center.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_CONTROL_CENTER_BUILD_DIR) && mkdir -p $(XXRI_CONTROL_CENTER_BUILD_DIR)
	@$(call world/execute, XXRI_CONTROL_CENTER, $(CROSS_CC) $(XXRI_OS_CFLAGS) -w $(XXRI_GTK3_CFLAGS) $(XXRI_CONTROL_CENTER_DIR)/xxri-control-center.c -o $(XXRI_CONTROL_CENTER_BUILD_DIR)/xxri-control-center $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lX11 -lm -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-control-center.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-control-center.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-control-center)
	@$(call install_fixup, xxri-control-center,PRIORITY,optional)
	@$(call install_fixup, xxri-control-center,SECTION,base)
	@$(call install_fixup, xxri-control-center,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-control-center,DESCRIPTION,"quick settings panel")

	@$(call install_copy, xxri-control-center, 0, 0, 0755, $(XXRI_CONTROL_CENTER_BUILD_DIR)/xxri-control-center, /usr/local/bin/xxri-control-center)
	@$(call install_tree, xxri-control-center, 0, 0, $(XXRI_CONTROL_CENTER_DIR)/root, /)

	@$(call install_finish, xxri-control-center)

	@$(call touch)

# vim: syntax=make
