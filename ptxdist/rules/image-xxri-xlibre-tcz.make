# -*-makefile-*-
#
# xxri-xlibre.tcz: the XLibre X server (Xorg + modesetting, Xfbdev) and its
# libinput driver, loaded at boot in place of Tiny Core's xorg-server.tcz
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_XLIBRE_TCZ) += image-xxri-xlibre-tcz

IMAGE_XXRI_XLIBRE_TCZ		:= image-xxri-xlibre-tcz
IMAGE_XXRI_XLIBRE_TCZ_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_XLIBRE_TCZ)
IMAGE_XXRI_XLIBRE_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-xlibre.tcz
IMAGE_XXRI_XLIBRE_TCZ_PKGS	:= $(XXRI_XLIBRE_TCZ_PKGS)
IMAGE_XXRI_XLIBRE_TCZ_DEP	:= $(PTXDIST_WORKSPACE)/config/cde/dep/xxri-xlibre.tcz.dep

ifdef PTXCONF_IMAGE_XXRI_XLIBRE_TCZ
$(IMAGE_XXRI_XLIBRE_TCZ_IMAGE): $(IMAGE_XXRI_XLIBRE_TCZ_DEP)
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_XLIBRE_TCZ) \
		ptxd_make_xxri_tcz "$(shell cat $(IMAGE_XXRI_XLIBRE_TCZ_DEP))"
	@$(call finish)
endif

# vim: syntax=make
