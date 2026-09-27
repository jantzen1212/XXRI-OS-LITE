# -*-makefile-*-
#
# core.gz: Tiny Core 16.0's initramfs with the XXRI packages over it
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_CORE) += image-xxri-core

IMAGE_XXRI_CORE		:= image-xxri-core
IMAGE_XXRI_CORE_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_CORE)
IMAGE_XXRI_CORE_IMAGE	:= $(IMAGEDIR)/core.gz
IMAGE_XXRI_CORE_PKGS	:= $(XXRI_CORE_PKGS)

ifdef PTXCONF_IMAGE_XXRI_CORE
$(IMAGE_XXRI_CORE_IMAGE): $(STATEDIR)/xxri-tc-base.install $(XXRI_TC_BASE_REMOVE) \
		$(PTXDIST_WORKSPACE)/scripts/xxri-mkinitramfs.py $(STATEDIR)/host-python3.install.post
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_CORE) XXRI_HOST_PYTHON=$(XXRI_HOST_PYTHON) \
		ptxd_make_xxri_core "$(XXRI_TC_BASE_SOURCE)" "$(XXRI_TC_BASE_REMOVE)"
	@$(call finish)
endif

# vim: syntax=make
