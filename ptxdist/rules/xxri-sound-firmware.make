# -*-makefile-*-
#
# xxri-sound-firmware - x86_64 only: firmware for the sound modules xxri-kernel
# adds, packaged in xxri-sound.tcz (image-xxri-sound-tcz).
#   Intel SOF DSP firmware and topologies: sof-bin, laid out like its install.sh
#     does (FW_DEST=/usr/local/lib/firmware/intel). 2025.01.1 is the release
#     for the 6.12 kernel; newer topologies can need newer kernel tokens.
#   HDA speaker amps (Cirrus CS35L41/CS35L56, TI TAS2781/TAS2563): the files in
#     config/firmware/sound-x86_64.list, picked from linux-firmware's WHENCE by
#     scripts/xxri-fw-select.py.
# ", n": firmware has to stay bit-identical, so no stripping.
#
PACKAGES-$(PTXCONF_XXRI_SOUND_FIRMWARE) += xxri-sound-firmware

ifeq ($(PTXCONF_ARCH_X86_64),y)

XXRI_SOUND_FIRMWARE_VERSION	:= 2025.01.1
XXRI_SOUND_FIRMWARE_SHA256	:= a36210d9c245e81b0d9674d6b27d1fd4122968cf925aefc55b486d3650f88323
XXRI_SOUND_FIRMWARE		:= sof-bin-$(XXRI_SOUND_FIRMWARE_VERSION)
XXRI_SOUND_FIRMWARE_SUFFIX	:= tar.gz
XXRI_SOUND_FIRMWARE_URL		:= https://github.com/thesofproject/sof-bin/releases/download/v$(XXRI_SOUND_FIRMWARE_VERSION)/$(XXRI_SOUND_FIRMWARE).$(XXRI_SOUND_FIRMWARE_SUFFIX)
XXRI_SOUND_FIRMWARE_SOURCE	:= $(SRCDIR)/$(XXRI_SOUND_FIRMWARE).$(XXRI_SOUND_FIRMWARE_SUFFIX)
XXRI_SOUND_FIRMWARE_DIR		:= $(BUILDDIR)/$(XXRI_SOUND_FIRMWARE)
XXRI_SOUND_FIRMWARE_LICENSE	:= LicenseRef-Intel-SOF-firmware AND LicenseRef-cirrus-firmware AND LicenseRef-ti-tas2781-firmware
XXRI_SOUND_FIRMWARE_DEVPKG	:= NO
XXRI_SOUND_FIRMWARE_LIST	:= $(PTXDIST_WORKSPACE)/config/firmware/sound-x86_64.list
XXRI_SOUND_FIRMWARE_SELECT	:= $(PTXDIST_WORKSPACE)/scripts/xxri-fw-select.py
XXRI_SOUND_FIRMWARE_STAGE	:= $(XXRI_SOUND_FIRMWARE_DIR)-stage

# the amp files come out of the linux-firmware tarball the GPU firmware rules
# already fetch
$(STATEDIR)/xxri-sound-firmware.prepare: $(XXRI_SOUND_FIRMWARE_LIST) $(XXRI_SOUND_FIRMWARE_SELECT) \
		$(STATEDIR)/xxri-firmware-intel.get
	@$(call targetinfo)
	@rm -rf $(XXRI_SOUND_FIRMWARE_STAGE) && mkdir -p $(XXRI_SOUND_FIRMWARE_STAGE)/firmware/intel
	@cd $(XXRI_SOUND_FIRMWARE_DIR) && cp -a sof sof-ipc4 sof-ipc4-lib sof-ipc4-tplg sof-ace-tplg sof-tplg \
		$(XXRI_SOUND_FIRMWARE_STAGE)/firmware/intel/
	@$(XXRI_HOST_PYTHON) $(XXRI_SOUND_FIRMWARE_SELECT) extract $(XXRI_LINUX_FIRMWARE_SOURCE) \
		$(XXRI_SOUND_FIRMWARE_STAGE)/firmware $(XXRI_SOUND_FIRMWARE_LIST)
	@$(call touch)

$(STATEDIR)/xxri-sound-firmware.compile $(STATEDIR)/xxri-sound-firmware.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-sound-firmware.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-sound-firmware)
	@$(call install_fixup, xxri-sound-firmware,PRIORITY,optional)
	@$(call install_fixup, xxri-sound-firmware,SECTION,base)
	@$(call install_fixup, xxri-sound-firmware,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-sound-firmware,DESCRIPTION,"SOF DSP and HDA amplifier firmware")
	@$(call install_tree, xxri-sound-firmware, 0, 0, $(XXRI_SOUND_FIRMWARE_STAGE)/firmware, /usr/local/lib/firmware, n)
#	# the redistribution terms travel with the files
	@$(call install_tree, xxri-sound-firmware, 0, 0, $(XXRI_SOUND_FIRMWARE_STAGE)/licenses, \
		/usr/local/share/doc/xxri-sound-firmware, n)
	@$(foreach f, LICENCE.Intel LICENCE.NXP Notice.NXP README.Intel, \
		$(call install_copy, xxri-sound-firmware, 0, 0, 0644, $(XXRI_SOUND_FIRMWARE_DIR)/$(f), \
			/usr/local/share/doc/xxri-sound-firmware/sof-bin/$(f))$(ptx/nl))
	@$(call install_copy, xxri-sound-firmware, 0, 0, 0644, $(XXRI_SOUND_FIRMWARE_LIST), \
		/usr/local/share/doc/xxri-sound-firmware/sound-x86_64.list)
	@$(call install_finish, xxri-sound-firmware)
	@$(call touch)

else
# i686: nothing (the i686 system keeps Tiny Core's alsa-modules)
$(STATEDIR)/xxri-sound-firmware.get $(STATEDIR)/xxri-sound-firmware.extract \
$(STATEDIR)/xxri-sound-firmware.prepare $(STATEDIR)/xxri-sound-firmware.compile \
$(STATEDIR)/xxri-sound-firmware.install $(STATEDIR)/xxri-sound-firmware.targetinstall:
	@$(call targetinfo)
	@$(call touch)
endif

# vim: syntax=make
