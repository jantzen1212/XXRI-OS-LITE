# -*-makefile-*-
#
# xxri-firmware-intel.tcz: config/firmware/intel.list, loaded at boot
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_FIRMWARE_INTEL_TCZ) += image-xxri-firmware-intel-tcz

IMAGE_XXRI_FIRMWARE_INTEL_TCZ		:= image-xxri-firmware-intel-tcz
IMAGE_XXRI_FIRMWARE_INTEL_TCZ_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_FIRMWARE_INTEL_TCZ)
IMAGE_XXRI_FIRMWARE_INTEL_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-firmware-intel.tcz
IMAGE_XXRI_FIRMWARE_INTEL_TCZ_PKGS	:= xxri-firmware-intel

ifdef PTXCONF_IMAGE_XXRI_FIRMWARE_INTEL_TCZ
$(IMAGE_XXRI_FIRMWARE_INTEL_TCZ_IMAGE):
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_FIRMWARE_INTEL_TCZ) ptxd_make_xxri_tcz ""
	@$(call finish)
endif

# vim: syntax=make
