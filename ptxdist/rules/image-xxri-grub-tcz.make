# -*-makefile-*-
#
# xxri-grub.tcz: GRUB 2.14 utilities and module trees for the installer
# (loaded on demand by xxri-install-backend, not at boot)
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_GRUB_TCZ) += image-xxri-grub-tcz

IMAGE_XXRI_GRUB_TCZ		:= image-xxri-grub-tcz
IMAGE_XXRI_GRUB_TCZ_DIR		:= $(BUILDDIR)/$(IMAGE_XXRI_GRUB_TCZ)
IMAGE_XXRI_GRUB_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-grub.tcz
IMAGE_XXRI_GRUB_TCZ_PKGS	:= $(XXRI_GRUB_TCZ_PKGS)

ifdef PTXCONF_IMAGE_XXRI_GRUB_TCZ
$(IMAGE_XXRI_GRUB_TCZ_IMAGE):
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_GRUB_TCZ) ptxd_make_xxri_tcz ""
	@$(call finish)
endif

# vim: syntax=make
