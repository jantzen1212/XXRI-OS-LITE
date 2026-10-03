# -*-makefile-*-
PACKAGES-$(PTXCONF_XXRI_CDE) += xxri-cde

XXRI_CDE_VERSION	:= 2.6
XXRI_CDE		:= xxri-cde-$(XXRI_CDE_VERSION)
XXRI_CDE_LICENSE	:= various (see each extension's .info in the Tiny Core repository)
XXRI_CDE_CACHE		:= $(SRCDIR)/xxri-cde
XXRI_CDE_DIR		:= $(IMAGEDIR)/xxri-cde
# A change to the set re-runs the package (PTXdist does not watch these
# files): only the ISO depends on it, so nothing else is rebuilt.
XXRI_CDE_CONFIG		:= $(PTXDIST_WORKSPACE)/config/cde/manifest \
	$(wildcard $(PTXDIST_WORKSPACE)/config/cde/*.lst $(PTXDIST_WORKSPACE)/config/cde/dep/*.dep) \
	$(PTXDIST_WORKSPACE)/scripts/xxri-fetch-cde.sh

$(STATEDIR)/xxri-cde.get: $(XXRI_CDE_CONFIG)
	@$(call targetinfo)
	@rm -rf $(XXRI_CDE_DIR).tmp
	@PATH=$(PTXDIST_SYSROOT_HOST)/bin:$$PATH $(PTXDIST_WORKSPACE)/scripts/xxri-fetch-cde.sh \
		$(PTXDIST_WORKSPACE)/config/cde $(XXRI_CDE_CACHE) \
		$(XXRI_CDE_DIR).tmp
	@$(call touch)

$(STATEDIR)/xxri-cde.extract $(STATEDIR)/xxri-cde.prepare \
$(STATEDIR)/xxri-cde.compile $(STATEDIR)/xxri-cde.targetinstall:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-cde.install:
	@$(call targetinfo)
	@rm -rf $(XXRI_CDE_DIR) && mv $(XXRI_CDE_DIR).tmp $(XXRI_CDE_DIR)
	@$(call touch)
