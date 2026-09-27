# -*-makefile-*-
#
# xxri-store - built from the repository's src/xxri-store
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_STORE) += xxri-store

#
# Paths and names
#
XXRI_STORE_VERSION	:= $(XXRI_OS_VERSION)
XXRI_STORE		:= xxri-store-$(XXRI_STORE_VERSION)
XXRI_STORE_URL		:= file://$(XXRI_SRC)/xxri-store
XXRI_STORE_DIR		:= $(BUILDDIR)/$(XXRI_STORE)
XXRI_STORE_BUILD_DIR	:= $(XXRI_STORE_DIR)-build
XXRI_STORE_LICENSE	:= $(XXRI_LICENSE)
XXRI_STORE_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_STORE_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_STORE_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-store.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_STORE_BUILD_DIR) && mkdir -p $(XXRI_STORE_BUILD_DIR)
	@$(call world/execute, XXRI_STORE, $(CROSS_CXX) $(XXRI_OS_CFLAGS) -w -fpermissive $(XXRI_GTK3_CFLAGS) $(XXRI_STORE_DIR)/xxri-store-gui.c -o $(XXRI_STORE_BUILD_DIR)/xxri-store-gui $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-store.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-store.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-store)
	@$(call install_fixup, xxri-store,PRIORITY,optional)
	@$(call install_fixup, xxri-store,SECTION,base)
	@$(call install_fixup, xxri-store,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-store,DESCRIPTION,"Store")

	@$(call install_copy, xxri-store, 0, 0, 0755, $(XXRI_STORE_BUILD_DIR)/xxri-store-gui, /usr/local/bin/xxri-store-gui)
	@$(call install_tree, xxri-store, 0, 0, $(XXRI_STORE_DIR)/root, /)

	@$(call install_finish, xxri-store)

	@$(call touch)

# vim: syntax=make
