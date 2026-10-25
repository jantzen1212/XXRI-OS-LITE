# -*-makefile-*-
PACKAGES-$(PTXCONF_XXRI_CDE) += xxri-cde

XXRI_CDE_VERSION	:= 2.6
XXRI_CDE		:= xxri-cde-$(XXRI_CDE_VERSION)
XXRI_CDE_LICENSE	:= various (see each extension's .info in the Tiny Core repository)
XXRI_CDE_DIR		:= $(IMAGEDIR)/xxri-cde
# A change to the set re-runs the package (PTXdist does not watch these
# files): only the ISO depends on it, so nothing else is rebuilt.
# The two architectures' packages share names, so each has its own download cache.
ifeq ($(PTXCONF_ARCH_X86_64),y)
XXRI_CDE_CACHE		:= $(SRCDIR)/xxri-cde64
else
XXRI_CDE_CACHE		:= $(SRCDIR)/xxri-cde
endif
XXRI_CDE_CONFIG		:= $(XXRI_CDE_CONFIG_DIR)/manifest \
	$(wildcard $(XXRI_CDE_CONFIG_DIR)/*.lst $(XXRI_CDE_CONFIG_DIR)/dep/*.dep) \
	$(PTXDIST_WORKSPACE)/scripts/xxri-fetch-cde.sh
XXRI_CDE_FETCH		= PATH=$(PTXDIST_SYSROOT_HOST)/bin:$$PATH $(PTXDIST_WORKSPACE)/scripts/xxri-fetch-cde.sh \
	$(XXRI_CDE_CONFIG_DIR) $(XXRI_CDE_CACHE) $(XXRI_CDE_DIR).tmp $(XXRI_TC_ARCH)

# .get fills the download cache, .install builds cde/ from it (md5 checked again,
# downloads only what is missing), so .install can run again on its own.
$(STATEDIR)/xxri-cde.get: $(XXRI_CDE_CONFIG)
	@$(call targetinfo)
	@rm -rf $(XXRI_CDE_DIR).tmp
	@$(XXRI_CDE_FETCH)
	@rm -rf $(XXRI_CDE_DIR).tmp
	@$(call touch)

$(STATEDIR)/xxri-cde.extract $(STATEDIR)/xxri-cde.prepare \
$(STATEDIR)/xxri-cde.compile $(STATEDIR)/xxri-cde.targetinstall:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-cde.install:
	@$(call targetinfo)
	@rm -rf $(XXRI_CDE_DIR).tmp
	@$(XXRI_CDE_FETCH)
	@rm -rf $(XXRI_CDE_DIR) && mv $(XXRI_CDE_DIR).tmp $(XXRI_CDE_DIR)
	@$(call touch)
