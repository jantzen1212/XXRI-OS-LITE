# -*-makefile-*-
#
# xxri-firmware-gpu.tcz: config/firmware/gpu.list, loaded at boot
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_FIRMWARE_GPU_TCZ) += image-xxri-firmware-gpu-tcz

IMAGE_XXRI_FIRMWARE_GPU_TCZ		:= image-xxri-firmware-gpu-tcz
IMAGE_XXRI_FIRMWARE_GPU_TCZ_DIR		:= $(BUILDDIR)/$(IMAGE_XXRI_FIRMWARE_GPU_TCZ)
IMAGE_XXRI_FIRMWARE_GPU_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-firmware-gpu.tcz
IMAGE_XXRI_FIRMWARE_GPU_TCZ_PKGS	:= xxri-firmware-gpu

ifdef PTXCONF_IMAGE_XXRI_FIRMWARE_GPU_TCZ
$(IMAGE_XXRI_FIRMWARE_GPU_TCZ_IMAGE):
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_FIRMWARE_GPU_TCZ) ptxd_make_xxri_tcz ""
	@$(call finish)
endif

# vim: syntax=make
