# -*-makefile-*-
#
# xxri-deps - built from the repository's src/xxri-deps
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_DEPS) += xxri-deps

#
# Paths and names
#
XXRI_DEPS_VERSION	:= $(XXRI_OS_VERSION)
XXRI_DEPS		:= xxri-deps-$(XXRI_DEPS_VERSION)
XXRI_DEPS_URL		:= file://$(XXRI_SRC)/xxri-deps
XXRI_DEPS_DIR		:= $(BUILDDIR)/$(XXRI_DEPS)
XXRI_DEPS_BUILD_DIR	:= $(XXRI_DEPS_DIR)-build
XXRI_DEPS_LICENSE	:= $(XXRI_LICENSE)
XXRI_DEPS_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-deps.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_DEPS_BUILD_DIR) && mkdir -p $(XXRI_DEPS_BUILD_DIR)
	@$(call world/execute, XXRI_DEPS, $(CROSS_CC) $(XXRI_OS_CFLAGS) $(XXRI_DEPS_DIR)/xxri-deps-scan.c -o $(XXRI_DEPS_BUILD_DIR)/xxri-deps-scan)
	@$(call touch)

$(STATEDIR)/xxri-deps.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-deps.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-deps)
	@$(call install_fixup, xxri-deps,PRIORITY,optional)
	@$(call install_fixup, xxri-deps,SECTION,base)
	@$(call install_fixup, xxri-deps,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-deps,DESCRIPTION,"AppImage host-library dependency scan")

	@$(call install_copy, xxri-deps, 0, 0, 0755, $(XXRI_DEPS_BUILD_DIR)/xxri-deps-scan, /usr/local/bin/xxri-deps-scan)

	@$(call install_finish, xxri-deps)

	@$(call touch)

# vim: syntax=make
