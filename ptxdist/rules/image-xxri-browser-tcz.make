# -*-makefile-*-
#
# xxri-browser.tcz: the browser and its private Qt, a Tiny Core extension
# loaded at boot (onboot.lst); its .dep lists the Tiny Core extensions whose
# libraries it links against
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_BROWSER_TCZ) += image-xxri-browser-tcz

IMAGE_XXRI_BROWSER_TCZ		:= image-xxri-browser-tcz
IMAGE_XXRI_BROWSER_TCZ_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_BROWSER_TCZ)
IMAGE_XXRI_BROWSER_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-browser.tcz
IMAGE_XXRI_BROWSER_TCZ_PKGS	:= $(XXRI_BROWSER_TCZ_PKGS)
IMAGE_XXRI_BROWSER_TCZ_DEP	:= $(PTXDIST_WORKSPACE)/config/cde/dep/xxri-browser.tcz.dep

ifdef PTXCONF_IMAGE_XXRI_BROWSER_TCZ
$(IMAGE_XXRI_BROWSER_TCZ_IMAGE): $(IMAGE_XXRI_BROWSER_TCZ_DEP)
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_BROWSER_TCZ) \
		ptxd_make_xxri_tcz "$(shell cat $(IMAGE_XXRI_BROWSER_TCZ_DEP))"
	@$(call finish)
endif

# vim: syntax=make
