# -*-makefile-*-
#
# xxri-firmware-nvidia.tcz: config/firmware/nvidia.list, loaded at boot
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ) += image-xxri-firmware-nvidia-tcz

IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ		:= image-xxri-firmware-nvidia-tcz
IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ)
IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-firmware-nvidia.tcz
IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ_PKGS	:= xxri-firmware-nvidia

ifdef PTXCONF_IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ
$(IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ_IMAGE):
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_FIRMWARE_NVIDIA_TCZ) ptxd_make_xxri_tcz ""
	@$(call finish)
endif

# vim: syntax=make
