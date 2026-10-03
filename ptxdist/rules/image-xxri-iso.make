# -*-makefile-*-
#
# The XXRI OS Lite ISO: kernel, core.gz, the early CPU microcode images, the
# extension set (Tiny Core's, pinned, plus XXRI's own .tcz) and GRUB 2.14 for
# BIOS and IA32 UEFI.
# Assembly: scripts/xxri-mkiso.sh.  The Corresponding Source of every
# package goes to images/sources/.
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_ISO) += image-xxri-iso

IMAGE_XXRI_ISO		:= image-xxri-iso
IMAGE_XXRI_ISO_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_ISO)
IMAGE_XXRI_ISO_IMAGE	:= $(IMAGEDIR)/XXRI-Lite-$(XXRI_OS_VERSION).iso

# The Corresponding Source, published in images/sources/ next to the ISO (the
# ISO's boot/grub/SOURCE.txt points there): the source archives of every
# package XXRI builds and ships - all parts of each, so every Qt module - and
# of the GRUB boot images; XXRI's own programs, patches and build project as
# one archive; and where Tiny Core publishes the sources of its unmodified
# binaries.  Deferred (=): the package variables are defined in later files.
IMAGE_XXRI_ISO_SRC_PKGS	= $(XXRI_CORE_PKGS) $(XXRI_BROWSER_TCZ_PKGS) $(XXRI_XLIBRE_TCZ_PKGS) \
			  $(XXRI_GRUB_TCZ_PKGS) host-xxri-grub
IMAGE_XXRI_ISO_SOURCES	= $(sort $(wildcard $(foreach pkg,$(IMAGE_XXRI_ISO_SRC_PKGS), \
	$(foreach part,$($(PTX_MAP_TO_PACKAGE_$(pkg))_PARTS),$($(part)_SOURCE)))))

ifdef PTXCONF_IMAGE_XXRI_ISO
# The other images by file name: prerequisites are expanded when this file is
# read, and PTXdist reads image-xxri-xlibre-tcz.make after this one, so
# $(IMAGE_XXRI_XLIBRE_TCZ_IMAGE) would still be empty here.
IMAGE_XXRI_ISO_TCZ	:= xxri-browser.tcz xxri-xlibre.tcz xxri-grub.tcz \
	xxri-firmware-gpu.tcz xxri-firmware-intel.tcz xxri-firmware-amd.tcz xxri-firmware-nvidia.tcz \
	xxri-firmware-wireless.tcz
$(IMAGE_XXRI_ISO_IMAGE): $(IMAGEDIR)/core.gz $(addprefix $(IMAGEDIR)/,$(IMAGE_XXRI_ISO_TCZ)) \
		$(PTXDIST_WORKSPACE)/scripts/xxri-mkiso.sh \
		$(STATEDIR)/host-xxri-grub.install.post $(STATEDIR)/host-xxri-xorriso.install.post \
		$(STATEDIR)/host-dosfstools.install.post $(STATEDIR)/host-mtools.install.post \
		$(STATEDIR)/xxri-cde.install $(STATEDIR)/xxri-kernel.install \
		$(STATEDIR)/xxri-ucode.install
	@$(call targetinfo)
	@rm -rf $(IMAGE_XXRI_ISO_DIR) && mkdir -p $(IMAGE_XXRI_ISO_DIR)
#	# the extension set: Tiny Core's (hard links, no copy) + XXRI's own
	@cp -al $(XXRI_CDE_DIR) $(IMAGE_XXRI_ISO_DIR)/cde
	@for t in $(IMAGE_XXRI_ISO_TCZ); do \
		cp $(IMAGEDIR)/$$t $(IMAGEDIR)/$$t.md5.txt $(IMAGE_XXRI_ISO_DIR)/cde/optional/ || exit 1; \
		if [ -f $(IMAGEDIR)/$$t.dep ]; then cp $(IMAGEDIR)/$$t.dep $(IMAGE_XXRI_ISO_DIR)/cde/optional/; fi; \
	done
	@PATH=$(PTXDIST_SYSROOT_HOST)/bin:$(PTXDIST_SYSROOT_HOST)/sbin:$$PATH \
		$(PTXDIST_WORKSPACE)/scripts/xxri-mkiso.sh $(XXRI_OS_VERSION) $@ \
		$(IMAGE_XXRI_CORE_IMAGE) $(XXRI_KERNEL_IMAGE) $(IMAGE_XXRI_ISO_DIR)/cde \
		$(HOST_XXRI_GRUB_PREFIX) $(HOST_XXRI_GRUB_SOURCE) --ucode $(XXRI_UCODE_OUT)
	@rm -rf $(IMAGE_XXRI_ISO_DIR)
#	# Corresponding Source (see above)
	@rm -rf $(IMAGEDIR)/sources && mkdir -p $(IMAGEDIR)/sources
	@for f in $(IMAGE_XXRI_ISO_SOURCES) $(wildcard $(HOST_XXRI_GRUB_SOURCE).sig); do \
		[ -f "$$f" ] || continue; \
		cp -p $$f $(IMAGEDIR)/sources/ || exit 1; \
	done
	@cd $(PTXDIST_WORKSPACE)/.. && tar --sort=name --mtime=@$(SOURCE_DATE_EPOCH) \
		--owner=0 --group=0 --numeric-owner --format=gnu \
		--exclude=platform-* --exclude=ptxdist/src --exclude=selected_* \
		-cf - ptxdist src | xz -9 -T1 > $(IMAGEDIR)/sources/xxri-$(XXRI_OS_VERSION)-src.tar.xz
	@cp $(PTXDIST_WORKSPACE)/config/sources/README $(IMAGEDIR)/sources/README
	@cd $(IMAGEDIR)/sources && sha256sum $$(ls | grep -vx SHA256SUMS) > SHA256SUMS
	@$(call finish)
endif

# vim: syntax=make
