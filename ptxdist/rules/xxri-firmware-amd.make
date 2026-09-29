# -*-makefile-*-
#
# xxri-firmware-amd - firmware for AMD GPUs up to GCN 3: radeon Southern/Sea
# Islands (HD 7000, R7/R9 200/300) and amdgpu Tonga, Fiji, Topaz, Carrizo,
# Stoney.
# Only the files config/firmware/amd.list names, resolved through the
# release's WHENCE by scripts/xxri-fw-select.py (an unknown name fails the
# build). Loaded at boot, before the KMS drivers; Tiny Core's udev helper
# serves /usr/local/lib/firmware through the sysfs fallback.
#
# ", n": no stripping - PTXdist strips ELF files, and firmware must stay
# bit-identical (some images are ELF, e.g. NVIDIA's GSP-RM).
#
PACKAGES-$(PTXCONF_XXRI_FIRMWARE_AMD) += xxri-firmware-amd

XXRI_FIRMWARE_AMD_VERSION	:= $(XXRI_LINUX_FIRMWARE_VERSION)
XXRI_FIRMWARE_AMD_SHA256	:= $(XXRI_LINUX_FIRMWARE_SHA256)
XXRI_FIRMWARE_AMD		:= xxri-firmware-amd-$(XXRI_FIRMWARE_AMD_VERSION)
XXRI_FIRMWARE_AMD_URL		:= $(XXRI_LINUX_FIRMWARE_URL)
XXRI_FIRMWARE_AMD_SOURCE	:= $(XXRI_LINUX_FIRMWARE_SOURCE)
XXRI_FIRMWARE_AMD_DIR		:= $(BUILDDIR)/$(XXRI_FIRMWARE_AMD)
XXRI_FIRMWARE_AMD_LICENSE	:= LicenseRef-amdgpu-firmware AND LicenseRef-radeon-firmware
XXRI_FIRMWARE_AMD_DEVPKG	:= NO
XXRI_FIRMWARE_AMD_LIST	:= $(PTXDIST_WORKSPACE)/config/firmware/amd.list
XXRI_FIRMWARE_AMD_SELECT	:= $(PTXDIST_WORKSPACE)/scripts/xxri-fw-select.py

# 650 MB tarball: nothing is unpacked in extract. The selection runs in
# prepare (host Python exists by then) and again when the list or script changes.
$(STATEDIR)/xxri-firmware-amd.extract:
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_AMD_DIR) && mkdir -p $(XXRI_FIRMWARE_AMD_DIR)
	@$(call touch)

$(STATEDIR)/xxri-firmware-amd.prepare: $(XXRI_FIRMWARE_AMD_LIST) $(XXRI_FIRMWARE_AMD_SELECT)
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_AMD_DIR)/firmware $(XXRI_FIRMWARE_AMD_DIR)/licenses
	@$(XXRI_HOST_PYTHON) $(XXRI_FIRMWARE_AMD_SELECT) extract $(XXRI_FIRMWARE_AMD_SOURCE) \
		$(XXRI_FIRMWARE_AMD_DIR)/firmware $(XXRI_FIRMWARE_AMD_LIST)
	@$(call touch)

$(STATEDIR)/xxri-firmware-amd.compile $(STATEDIR)/xxri-firmware-amd.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-firmware-amd.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-firmware-amd)
	@$(call install_fixup, xxri-firmware-amd,PRIORITY,optional)
	@$(call install_fixup, xxri-firmware-amd,SECTION,base)
	@$(call install_fixup, xxri-firmware-amd,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-firmware-amd,DESCRIPTION,"firmware for AMD GPUs up to GCN 3 (radeon SI and CIK and amdgpu VI)")
	@$(call install_tree, xxri-firmware-amd, 0, 0, $(XXRI_FIRMWARE_AMD_DIR)/firmware, /usr/local/lib/firmware, n)
#	# the redistribution terms travel with the files
	@$(call install_tree, xxri-firmware-amd, 0, 0, $(XXRI_FIRMWARE_AMD_DIR)/licenses, /usr/local/share/doc/xxri-firmware-amd, n)
	@$(call install_copy, xxri-firmware-amd, 0, 0, 0644, $(XXRI_FIRMWARE_AMD_LIST), \
		/usr/local/share/doc/xxri-firmware-amd/amd.list)
	@$(call install_finish, xxri-firmware-amd)
	@$(call touch)

# vim: syntax=make
