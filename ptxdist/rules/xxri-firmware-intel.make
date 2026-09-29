# -*-makefile-*-
#
# xxri-firmware-intel - firmware for the Atom/Celeron-class Intel iGPUs
# after Gen9: Elkhart/Jasper Lake, Tiger Lake, Alder Lake-S/-P/-N, Twin Lake.
# Only the files config/firmware/intel.list names, resolved through the
# release's WHENCE by scripts/xxri-fw-select.py (an unknown name fails the
# build). Loaded at boot, before the KMS drivers; Tiny Core's udev helper
# serves /usr/local/lib/firmware through the sysfs fallback.
#
# ", n": no stripping - PTXdist strips ELF files, and firmware must stay
# bit-identical (some images are ELF, e.g. NVIDIA's GSP-RM).
#
PACKAGES-$(PTXCONF_XXRI_FIRMWARE_INTEL) += xxri-firmware-intel

XXRI_FIRMWARE_INTEL_VERSION	:= $(XXRI_LINUX_FIRMWARE_VERSION)
XXRI_FIRMWARE_INTEL_SHA256	:= $(XXRI_LINUX_FIRMWARE_SHA256)
XXRI_FIRMWARE_INTEL		:= xxri-firmware-intel-$(XXRI_FIRMWARE_INTEL_VERSION)
XXRI_FIRMWARE_INTEL_URL		:= $(XXRI_LINUX_FIRMWARE_URL)
XXRI_FIRMWARE_INTEL_SOURCE	:= $(XXRI_LINUX_FIRMWARE_SOURCE)
XXRI_FIRMWARE_INTEL_DIR		:= $(BUILDDIR)/$(XXRI_FIRMWARE_INTEL)
XXRI_FIRMWARE_INTEL_LICENSE	:= LicenseRef-i915-firmware AND LicenseRef-xe-firmware
XXRI_FIRMWARE_INTEL_DEVPKG	:= NO
XXRI_FIRMWARE_INTEL_LIST	:= $(PTXDIST_WORKSPACE)/config/firmware/intel.list
XXRI_FIRMWARE_INTEL_SELECT	:= $(PTXDIST_WORKSPACE)/scripts/xxri-fw-select.py

# 650 MB tarball: nothing is unpacked in extract. The selection runs in
# prepare (host Python exists by then) and again when the list or script changes.
$(STATEDIR)/xxri-firmware-intel.extract:
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_INTEL_DIR) && mkdir -p $(XXRI_FIRMWARE_INTEL_DIR)
	@$(call touch)

$(STATEDIR)/xxri-firmware-intel.prepare: $(XXRI_FIRMWARE_INTEL_LIST) $(XXRI_FIRMWARE_INTEL_SELECT)
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_INTEL_DIR)/firmware $(XXRI_FIRMWARE_INTEL_DIR)/licenses
	@$(XXRI_HOST_PYTHON) $(XXRI_FIRMWARE_INTEL_SELECT) extract $(XXRI_FIRMWARE_INTEL_SOURCE) \
		$(XXRI_FIRMWARE_INTEL_DIR)/firmware $(XXRI_FIRMWARE_INTEL_LIST)
	@$(call touch)

$(STATEDIR)/xxri-firmware-intel.compile $(STATEDIR)/xxri-firmware-intel.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-firmware-intel.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-firmware-intel)
	@$(call install_fixup, xxri-firmware-intel,PRIORITY,optional)
	@$(call install_fixup, xxri-firmware-intel,SECTION,base)
	@$(call install_fixup, xxri-firmware-intel,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-firmware-intel,DESCRIPTION,"firmware for Atom and Celeron-class Intel GPUs after Gen9")
	@$(call install_tree, xxri-firmware-intel, 0, 0, $(XXRI_FIRMWARE_INTEL_DIR)/firmware, /usr/local/lib/firmware, n)
#	# the redistribution terms travel with the files
	@$(call install_tree, xxri-firmware-intel, 0, 0, $(XXRI_FIRMWARE_INTEL_DIR)/licenses, /usr/local/share/doc/xxri-firmware-intel, n)
	@$(call install_copy, xxri-firmware-intel, 0, 0, 0644, $(XXRI_FIRMWARE_INTEL_LIST), \
		/usr/local/share/doc/xxri-firmware-intel/intel.list)
	@$(call install_finish, xxri-firmware-intel)
	@$(call touch)

# vim: syntax=make
