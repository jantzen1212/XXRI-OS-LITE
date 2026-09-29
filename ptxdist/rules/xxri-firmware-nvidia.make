# -*-makefile-*-
#
# xxri-firmware-nvidia - the firmware nouveau needs for Maxwell 2 (GTX 900)
# acceleration.
# Only the files config/firmware/nvidia.list names, resolved through the
# release's WHENCE by scripts/xxri-fw-select.py (an unknown name fails the
# build). Loaded at boot, before the KMS drivers; Tiny Core's udev helper
# serves /usr/local/lib/firmware through the sysfs fallback.
#
# ", n": no stripping - PTXdist strips ELF files, and firmware must stay
# bit-identical (some images are ELF, e.g. NVIDIA's GSP-RM).
#
PACKAGES-$(PTXCONF_XXRI_FIRMWARE_NVIDIA) += xxri-firmware-nvidia

XXRI_FIRMWARE_NVIDIA_VERSION	:= $(XXRI_LINUX_FIRMWARE_VERSION)
XXRI_FIRMWARE_NVIDIA_SHA256	:= $(XXRI_LINUX_FIRMWARE_SHA256)
XXRI_FIRMWARE_NVIDIA		:= xxri-firmware-nvidia-$(XXRI_FIRMWARE_NVIDIA_VERSION)
XXRI_FIRMWARE_NVIDIA_URL		:= $(XXRI_LINUX_FIRMWARE_URL)
XXRI_FIRMWARE_NVIDIA_SOURCE	:= $(XXRI_LINUX_FIRMWARE_SOURCE)
XXRI_FIRMWARE_NVIDIA_DIR		:= $(BUILDDIR)/$(XXRI_FIRMWARE_NVIDIA)
XXRI_FIRMWARE_NVIDIA_LICENSE	:= LicenseRef-nvidia-firmware
XXRI_FIRMWARE_NVIDIA_DEVPKG	:= NO
XXRI_FIRMWARE_NVIDIA_LIST	:= $(PTXDIST_WORKSPACE)/config/firmware/nvidia.list
XXRI_FIRMWARE_NVIDIA_SELECT	:= $(PTXDIST_WORKSPACE)/scripts/xxri-fw-select.py

# 650 MB tarball: nothing is unpacked in extract. The selection runs in
# prepare (host Python exists by then) and again when the list or script changes.
$(STATEDIR)/xxri-firmware-nvidia.extract:
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_NVIDIA_DIR) && mkdir -p $(XXRI_FIRMWARE_NVIDIA_DIR)
	@$(call touch)

$(STATEDIR)/xxri-firmware-nvidia.prepare: $(XXRI_FIRMWARE_NVIDIA_LIST) $(XXRI_FIRMWARE_NVIDIA_SELECT)
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_NVIDIA_DIR)/firmware $(XXRI_FIRMWARE_NVIDIA_DIR)/licenses
	@$(XXRI_HOST_PYTHON) $(XXRI_FIRMWARE_NVIDIA_SELECT) extract $(XXRI_FIRMWARE_NVIDIA_SOURCE) \
		$(XXRI_FIRMWARE_NVIDIA_DIR)/firmware $(XXRI_FIRMWARE_NVIDIA_LIST)
	@$(call touch)

$(STATEDIR)/xxri-firmware-nvidia.compile $(STATEDIR)/xxri-firmware-nvidia.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-firmware-nvidia.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-firmware-nvidia)
	@$(call install_fixup, xxri-firmware-nvidia,PRIORITY,optional)
	@$(call install_fixup, xxri-firmware-nvidia,SECTION,base)
	@$(call install_fixup, xxri-firmware-nvidia,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-firmware-nvidia,DESCRIPTION,"firmware for NVIDIA GPUs up to Maxwell (nouveau)")
	@$(call install_tree, xxri-firmware-nvidia, 0, 0, $(XXRI_FIRMWARE_NVIDIA_DIR)/firmware, /usr/local/lib/firmware, n)
#	# the redistribution terms travel with the files
	@$(call install_tree, xxri-firmware-nvidia, 0, 0, $(XXRI_FIRMWARE_NVIDIA_DIR)/licenses, /usr/local/share/doc/xxri-firmware-nvidia, n)
	@$(call install_copy, xxri-firmware-nvidia, 0, 0, 0644, $(XXRI_FIRMWARE_NVIDIA_LIST), \
		/usr/local/share/doc/xxri-firmware-nvidia/nvidia.list)
	@$(call install_finish, xxri-firmware-nvidia)
	@$(call touch)

# vim: syntax=make
