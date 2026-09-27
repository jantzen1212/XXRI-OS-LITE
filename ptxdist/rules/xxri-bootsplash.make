# -*-makefile-*-
#
# xxri-bootsplash - built from the repository's src/xxri-bootsplash
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_BOOTSPLASH) += xxri-bootsplash

#
# Paths and names
#
XXRI_BOOTSPLASH_VERSION	:= $(XXRI_OS_VERSION)
XXRI_BOOTSPLASH		:= xxri-bootsplash-$(XXRI_BOOTSPLASH_VERSION)
XXRI_BOOTSPLASH_URL		:= file://$(XXRI_SRC)/xxri-bootsplash
XXRI_BOOTSPLASH_DIR		:= $(BUILDDIR)/$(XXRI_BOOTSPLASH)
XXRI_BOOTSPLASH_BUILD_DIR	:= $(XXRI_BOOTSPLASH_DIR)-build
XXRI_BOOTSPLASH_LICENSE	:= $(XXRI_LICENSE)
XXRI_BOOTSPLASH_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-bootsplash.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_BOOTSPLASH_BUILD_DIR) && mkdir -p $(XXRI_BOOTSPLASH_BUILD_DIR)
	@$(call world/execute, XXRI_BOOTSPLASH, $(CROSS_CC) -march=i586 -mtune=generic -O2 -Wall -static $(XXRI_BOOTSPLASH_DIR)/xxri-bootsplash.c -o $(XXRI_BOOTSPLASH_BUILD_DIR)/xxri-bootsplash -lm)
	@$(call touch)

$(STATEDIR)/xxri-bootsplash.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-bootsplash.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-bootsplash)
	@$(call install_fixup, xxri-bootsplash,PRIORITY,optional)
	@$(call install_fixup, xxri-bootsplash,SECTION,base)
	@$(call install_fixup, xxri-bootsplash,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-bootsplash,DESCRIPTION,"framebuffer boot splash")

	@$(call install_copy, xxri-bootsplash, 0, 0, 0755, $(XXRI_BOOTSPLASH_BUILD_DIR)/xxri-bootsplash, /usr/local/sbin/xxri-bootsplash)
	@$(call install_tree, xxri-bootsplash, 0, 0, $(XXRI_BOOTSPLASH_DIR)/root, /)

	@$(call install_finish, xxri-bootsplash)

	@$(call touch)

# vim: syntax=make
