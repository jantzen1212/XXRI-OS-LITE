# -*-makefile-*-
#
# xxri-wctl - built from the repository's src/xxri-wctl
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_WCTL) += xxri-wctl

#
# Paths and names
#
XXRI_WCTL_VERSION	:= $(XXRI_OS_VERSION)
XXRI_WCTL		:= xxri-wctl-$(XXRI_WCTL_VERSION)
XXRI_WCTL_URL		:= file://$(XXRI_SRC)/xxri-wctl
XXRI_WCTL_DIR		:= $(BUILDDIR)/$(XXRI_WCTL)
XXRI_WCTL_BUILD_DIR	:= $(XXRI_WCTL_DIR)-build
XXRI_WCTL_LICENSE	:= $(XXRI_LICENSE)
XXRI_WCTL_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_WCTL_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_WCTL_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-wctl.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_WCTL_BUILD_DIR) && mkdir -p $(XXRI_WCTL_BUILD_DIR)
	@$(call world/execute, XXRI_WCTL, $(CROSS_CC) $(XXRI_OS_CFLAGS) -Wall -Wextra -std=gnu99 $(XXRI_WCTL_DIR)/xxri-wctl.c -o $(XXRI_WCTL_BUILD_DIR)/xxri-wctl $(XXRI_TC_LDFLAGS) $$($(XXRI_PKG_CONFIG) --libs x11))
	@$(call touch)

$(STATEDIR)/xxri-wctl.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-wctl.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-wctl)
	@$(call install_fixup, xxri-wctl,PRIORITY,optional)
	@$(call install_fixup, xxri-wctl,SECTION,base)
	@$(call install_fixup, xxri-wctl,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-wctl,DESCRIPTION,"window control helper")

	@$(call install_copy, xxri-wctl, 0, 0, 0755, $(XXRI_WCTL_BUILD_DIR)/xxri-wctl, /usr/local/bin/xxri-wctl)

	@$(call install_finish, xxri-wctl)

	@$(call touch)

# vim: syntax=make
