# -*-makefile-*-
#
# xxri-store-catalog - x86_64 only: the Store's x64.json, the offline copy under
# /usr/local/share/xxri-store used until the Store has fetched a newer one.
# scripts/xxri-store-catalog-sync.py fetches and checks it on every build and
# only rewrites the files when the catalog changed; if it cannot get a good
# catalog the build stops. The i686 Store fetches i686.json itself.
#
PACKAGES-$(PTXCONF_XXRI_STORE_CATALOG) += xxri-store-catalog

ifeq ($(PTXCONF_ARCH_X86_64),y)

XXRI_STORE_CATALOG_VERSION	:= $(XXRI_OS_VERSION)
XXRI_STORE_CATALOG		:= xxri-store-catalog-$(XXRI_STORE_CATALOG_VERSION)
XXRI_STORE_CATALOG_LICENSE	:= $(XXRI_LICENSE)
XXRI_STORE_CATALOG_DEVPKG	:= NO
XXRI_STORE_CATALOG_SYNC		:= $(PTXDIST_WORKSPACE)/scripts/xxri-store-catalog-sync.py
XXRI_STORE_CATALOG_DIR		:= $(BUILDDIR)/$(XXRI_STORE_CATALOG)

$(STATEDIR)/xxri-store-catalog.get $(STATEDIR)/xxri-store-catalog.extract \
$(STATEDIR)/xxri-store-catalog.prepare $(STATEDIR)/xxri-store-catalog.compile \
$(STATEDIR)/xxri-store-catalog.install:
	@$(call targetinfo)
	@$(call touch)

# the phony prerequisite makes this run every time; the script leaves the files alone when the
# catalog is the one already there, so their timestamps (and the stamp that
# depends on them) only move when it changed

.PHONY: xxri-store-catalog-always
xxri-store-catalog-always:

$(XXRI_STORE_CATALOG_DIR)/x64.json.source: xxri-store-catalog-always
	@$(call targetinfo)
	@mkdir -p $(XXRI_STORE_CATALOG_DIR)
	@$(XXRI_HOST_PYTHON) $(XXRI_STORE_CATALOG_SYNC) $(XXRI_STORE_CATALOG_DIR)

$(STATEDIR)/xxri-store-catalog.targetinstall: $(XXRI_STORE_CATALOG_DIR)/x64.json.source
	@$(call targetinfo)
	@$(call install_init, xxri-store-catalog)
	@$(call install_fixup, xxri-store-catalog,PRIORITY,optional)
	@$(call install_fixup, xxri-store-catalog,SECTION,base)
	@$(call install_fixup, xxri-store-catalog,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-store-catalog,DESCRIPTION,"Store catalog for x86_64")
	@$(call install_copy, xxri-store-catalog, 0, 0, 0644, $(XXRI_STORE_CATALOG_DIR)/x64.json, \
		/usr/local/share/xxri-store/x64.json)
	@$(call install_copy, xxri-store-catalog, 0, 0, 0644, $(XXRI_STORE_CATALOG_DIR)/x64.json.source, \
		/usr/local/share/xxri-store/x64.json.source)
	@$(call install_finish, xxri-store-catalog)
	@$(call touch)

else
# i686: nothing
$(STATEDIR)/xxri-store-catalog.get $(STATEDIR)/xxri-store-catalog.extract \
$(STATEDIR)/xxri-store-catalog.prepare $(STATEDIR)/xxri-store-catalog.compile \
$(STATEDIR)/xxri-store-catalog.install $(STATEDIR)/xxri-store-catalog.targetinstall:
	@$(call targetinfo)
	@$(call touch)
endif

# vim: syntax=make
