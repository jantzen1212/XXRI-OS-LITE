# -*-makefile-*-
#
# The XXRI OS Lite ISO: kernel, core.gz, the extension set (Tiny Core's,
# pinned, plus XXRI's own .tcz) and GRUB 2.14 for BIOS and IA32 UEFI.
# Assembly: scripts/xxri-mkiso.sh.  The Corresponding Source of every
# package goes to images/sources/.
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_ISO) += image-xxri-iso

IMAGE_XXRI_ISO		:= image-xxri-iso
IMAGE_XXRI_ISO_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_ISO)
IMAGE_XXRI_ISO_IMAGE	:= $(IMAGEDIR)/XXRI-Lite-$(XXRI_OS_VERSION).iso

ifdef PTXCONF_IMAGE_XXRI_ISO
# The other images by file name: prerequisites are expanded when this file is
# read, and PTXdist reads image-xxri-xlibre-tcz.make after this one, so
# $(IMAGE_XXRI_XLIBRE_TCZ_IMAGE) would still be empty here.
$(IMAGE_XXRI_ISO_IMAGE): $(IMAGEDIR)/core.gz $(IMAGEDIR)/xxri-browser.tcz \
		$(IMAGEDIR)/xxri-xlibre.tcz $(IMAGEDIR)/xxri-grub.tcz $(PTXDIST_WORKSPACE)/scripts/xxri-mkiso.sh \
		$(STATEDIR)/host-xxri-grub.install.post $(STATEDIR)/host-xxri-xorriso.install.post \
		$(STATEDIR)/host-dosfstools.install.post $(STATEDIR)/host-mtools.install.post \
		$(STATEDIR)/xxri-cde.install $(STATEDIR)/xxri-kernel.install
	@$(call targetinfo)
	@rm -rf $(IMAGE_XXRI_ISO_DIR) && mkdir -p $(IMAGE_XXRI_ISO_DIR)
#	# the extension set: Tiny Core's (hard links, no copy) + XXRI's own
	@cp -al $(XXRI_CDE_DIR) $(IMAGE_XXRI_ISO_DIR)/cde
	@for t in xxri-browser.tcz xxri-xlibre.tcz xxri-grub.tcz; do \
		cp $(IMAGEDIR)/$$t $(IMAGEDIR)/$$t.md5.txt $(IMAGE_XXRI_ISO_DIR)/cde/optional/ || exit 1; \
		if [ -f $(IMAGEDIR)/$$t.dep ]; then cp $(IMAGEDIR)/$$t.dep $(IMAGE_XXRI_ISO_DIR)/cde/optional/; fi; \
	done
	@PATH=$(PTXDIST_SYSROOT_HOST)/bin:$(PTXDIST_SYSROOT_HOST)/sbin:$$PATH \
		$(PTXDIST_WORKSPACE)/scripts/xxri-mkiso.sh $(XXRI_OS_VERSION) $@ \
		$(IMAGE_XXRI_CORE_IMAGE) $(XXRI_KERNEL_IMAGE) $(IMAGE_XXRI_ISO_DIR)/cde \
		$(HOST_XXRI_GRUB_PREFIX) $(HOST_XXRI_GRUB_SOURCE)
	@rm -rf $(IMAGE_XXRI_ISO_DIR)
	@$(call finish)
endif

# vim: syntax=make
