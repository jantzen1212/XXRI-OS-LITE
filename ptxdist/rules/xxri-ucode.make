# -*-makefile-*-
#
# xxri-ucode - early microcode images: $(IMAGEDIR)/ucode/{intel,amd}-ucode.img
# plus their licences, placed in /boot by scripts/xxri-mkiso.sh and chosen
# per CPU vendor by GRUB (SMBIOS) and by the installer (/proc/cpuinfo).
#
# The Tiny Core 6.12.11 kernel has CONFIG_MICROCODE=y (early loading,
# CONFIG_MICROCODE_INITRD32 on 32-bit) and no late loading, so the initrd is
# the only way microcode reaches the CPU.  It is skipped under a hypervisor
# (head32.c), so a VM cannot show it being applied.
#
PACKAGES-$(PTXCONF_XXRI_UCODE) += xxri-ucode

# Intel: the GitHub release archive (checksum of the archive as downloaded;
# identical over repeated downloads)
XXRI_UCODE_VERSION	:= 20260925
XXRI_UCODE_SHA256	:= 0ec19023dad1e6dd0c5909174b8dff2555b91cf01671e36271a1ef36655c0cfc
XXRI_UCODE		:= xxri-ucode-$(XXRI_UCODE_VERSION)
XXRI_UCODE_URL		:= https://github.com/intel/Intel-Linux-Processor-Microcode-Data-Files/archive/refs/tags/microcode-$(XXRI_UCODE_VERSION).tar.gz
XXRI_UCODE_SOURCE	:= $(SRCDIR)/intel-microcode-$(XXRI_UCODE_VERSION).tar.gz
XXRI_UCODE_DIR		:= $(BUILDDIR)/$(XXRI_UCODE)
XXRI_UCODE_LICENSE	:= LicenseRef-Intel-microcode AND LicenseRef-amd-ucode
XXRI_UCODE_DEVPKG	:= NO

# AMD: the same linux-firmware release as the GPU firmware
XXRI_UCODE_AMD_VERSION	:= $(XXRI_LINUX_FIRMWARE_VERSION)
XXRI_UCODE_AMD_SHA256	:= $(XXRI_LINUX_FIRMWARE_SHA256)
XXRI_UCODE_AMD_URL	:= $(XXRI_LINUX_FIRMWARE_URL)
XXRI_UCODE_AMD_SOURCE	:= $(XXRI_LINUX_FIRMWARE_SOURCE)
XXRI_UCODE_AMD_DIR	:= $(XXRI_UCODE_DIR)
XXRI_UCODE_PARTS	+= XXRI_UCODE_AMD
XXRI_UCODE_AMD_LIST	:= $(PTXDIST_WORKSPACE)/config/firmware/amd-ucode.list

XXRI_UCODE_OUT		:= $(IMAGEDIR)/ucode
XXRI_UCODE_SCRIPTS	:= $(PTXDIST_WORKSPACE)/scripts/xxri-mkucode.py \
			   $(PTXDIST_WORKSPACE)/scripts/xxri_cpio.py \
			   $(PTXDIST_WORKSPACE)/scripts/xxri-fw-select.py

$(STATEDIR)/xxri-ucode.extract:
	@$(call targetinfo)
	@rm -rf $(XXRI_UCODE_DIR) && mkdir -p $(XXRI_UCODE_DIR)
	@$(call touch)

# Intel: intel-ucode/ and intel-ucode-with-caveats/ (its one file, Broadwell-
# EP/EX, is marked for early loading only - the only loading this kernel
# does).  AMD: the containers of config/firmware/amd-ucode.list.
$(STATEDIR)/xxri-ucode.prepare: $(XXRI_UCODE_AMD_LIST) $(XXRI_UCODE_SCRIPTS)
	@$(call targetinfo)
	@rm -rf $(XXRI_UCODE_DIR)/intel $(XXRI_UCODE_DIR)/amd
	@mkdir -p $(XXRI_UCODE_DIR)/intel
	@tar -xzf $(XXRI_UCODE_SOURCE) -C $(XXRI_UCODE_DIR)/intel --strip-components=1 \
		--wildcards '*/intel-ucode/*' '*/intel-ucode-with-caveats/*' '*/license'
	@$(XXRI_HOST_PYTHON) $(PTXDIST_WORKSPACE)/scripts/xxri-fw-select.py extract \
		$(XXRI_UCODE_AMD_SOURCE) $(XXRI_UCODE_DIR)/amd/firmware $(XXRI_UCODE_AMD_LIST)
	@$(call touch)

$(STATEDIR)/xxri-ucode.compile:
	@$(call targetinfo)
	@$(XXRI_HOST_PYTHON) $(PTXDIST_WORKSPACE)/scripts/xxri-mkucode.py intel \
		$(XXRI_UCODE_DIR)/intel-ucode.img \
		$(XXRI_UCODE_DIR)/intel/intel-ucode/* $(XXRI_UCODE_DIR)/intel/intel-ucode-with-caveats/*
	@$(XXRI_HOST_PYTHON) $(PTXDIST_WORKSPACE)/scripts/xxri-mkucode.py amd \
		$(XXRI_UCODE_DIR)/amd-ucode.img $(XXRI_UCODE_DIR)/amd/firmware/amd-ucode/*.bin
	@$(call touch)

$(STATEDIR)/xxri-ucode.install:
	@$(call targetinfo)
	@rm -rf $(XXRI_UCODE_OUT) && mkdir -p $(XXRI_UCODE_OUT)
	@install -m 644 $(XXRI_UCODE_DIR)/intel-ucode.img $(XXRI_UCODE_DIR)/amd-ucode.img $(XXRI_UCODE_OUT)/
	@install -m 644 $(XXRI_UCODE_DIR)/intel/license $(XXRI_UCODE_OUT)/LICENSE.intel-ucode
	@install -m 644 $(XXRI_UCODE_DIR)/amd/licenses/LICENSE.amd-ucode $(XXRI_UCODE_OUT)/LICENSE.amd-ucode
	@$(call touch)

$(STATEDIR)/xxri-ucode.targetinstall:
	@$(call targetinfo)
	@$(call touch)

# vim: syntax=make
