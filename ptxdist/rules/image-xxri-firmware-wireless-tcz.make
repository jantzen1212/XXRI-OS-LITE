# -*-makefile-*-
#
# xxri-firmware-wireless.tcz: config/firmware/wireless.list and regulatory.db,
# loaded at boot
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ) += image-xxri-firmware-wireless-tcz

IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ	:= image-xxri-firmware-wireless-tcz
IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ)
IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-firmware-wireless.tcz
IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ_PKGS	:= xxri-firmware-wireless

ifdef PTXCONF_IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ
$(IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ_IMAGE):
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_FIRMWARE_WIRELESS_TCZ) ptxd_make_xxri_tcz ""
	@$(call finish)
endif

# vim: syntax=make
