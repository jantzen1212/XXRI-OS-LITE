# -*-makefile-*-
#
# xxri-firmware-amd.tcz: config/firmware/amd.list, loaded at boot
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_FIRMWARE_AMD_TCZ) += image-xxri-firmware-amd-tcz

IMAGE_XXRI_FIRMWARE_AMD_TCZ		:= image-xxri-firmware-amd-tcz
IMAGE_XXRI_FIRMWARE_AMD_TCZ_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_FIRMWARE_AMD_TCZ)
IMAGE_XXRI_FIRMWARE_AMD_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-firmware-amd.tcz
IMAGE_XXRI_FIRMWARE_AMD_TCZ_PKGS	:= xxri-firmware-amd

ifdef PTXCONF_IMAGE_XXRI_FIRMWARE_AMD_TCZ
$(IMAGE_XXRI_FIRMWARE_AMD_TCZ_IMAGE):
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_FIRMWARE_AMD_TCZ) ptxd_make_xxri_tcz ""
	@$(call finish)
endif

# vim: syntax=make
