# -*-makefile-*-
PACKAGES-$(PTXCONF_XXRI_KERNEL) += xxri-kernel

XXRI_KERNEL_LICENSE	:= GPL-2.0-only
XXRI_KERNEL_IMAGE	:= $(IMAGEDIR)/vmlinuz

ifeq ($(PTXCONF_ARCH_X86_64),y)
#
# x86_64: Tiny Core's own kernel source (linux-6.12.11-patched) and config plus
# config/kernel/sof-x86_64.config: ALSA SoC, Sound Open Firmware, SoundWire, AMD
# ACP mics and the HDA speaker amps, none of which their kernel has. Their module
# packages (graphics, wireless, ...) stay, so the image has to keep their ABI;
# scripts/xxri-kabi-check.py fails the build if it doesn't. The sound modules and
# whatever else the fragment adds (scripts/xxri-sound-modules.py) go to
# xxri-sound.tcz, which replaces alsa-modules-6.12.11-tinycore64.tcz.
#
XXRI_KERNEL_VERSION	:= 6.12.11
XXRI_KERNEL_SHA256	:= b32e4b0048eb485d4bfad06c7824b05a4b5ba80b4dfe84edab8179828a09cc00
XXRI_KERNEL		:= linux-$(XXRI_KERNEL_VERSION)
XXRI_KERNEL_SUFFIX	:= tar.xz
XXRI_KERNEL_URL		:= http://tinycorelinux.net/16.x/x86_64/release/src/kernel/linux-6.12.11-patched.tar.xz
XXRI_KERNEL_SOURCE	:= $(SRCDIR)/linux-6.12.11-patched.tar.xz
XXRI_KERNEL_DIR		:= $(BUILDDIR)/$(XXRI_KERNEL)
XXRI_KERNEL_BUILD_DIR	:= $(BUILDDIR)/$(XXRI_KERNEL)-build
XXRI_KERNEL_MODS_DIR	:= $(BUILDDIR)/$(XXRI_KERNEL)-modules
XXRI_KERNEL_RELEASE	:= 6.12.11-tinycore64
XXRI_KERNEL_CFG_DIR	:= $(PTXDIST_WORKSPACE)/config/kernel
# config-6.12.11-tinycore64 (sha256 a11faee0...) and Module.symvers-6.12.11-tinycore64.gz
# (111b8119...) are Tiny Core's, from the same directory as the source
XXRI_KERNEL_CONFIG	:= $(XXRI_KERNEL_CFG_DIR)/config-6.12.11-tinycore64 \
	$(XXRI_KERNEL_CFG_DIR)/sof-x86_64.config $(XXRI_KERNEL_CFG_DIR)/tc-modules-x86_64.list \
	$(XXRI_KERNEL_CFG_DIR)/tc-built-modules-x86_64.list \
	$(PTXDIST_WORKSPACE)/scripts/xxri-kabi-check.py $(PTXDIST_WORKSPACE)/scripts/xxri-sound-modules.py
# Tiny Core built with GCC 14, same generation as the OSELAS one. It is called
# by full path because PTXdist's wrappers in PATH would add the userspace
# hardening flags to the kernel. Host pkg-config for objtool's libelf (the
# PTXdist one answers for the target).
XXRI_KERNEL_MAKE	:= $(MAKE) -C $(XXRI_KERNEL_DIR) O=$(XXRI_KERNEL_BUILD_DIR) ARCH=x86_64 \
	CROSS_COMPILE=$(PTXDIST_PLATFORMDIR)/selected_toolchain/$(PTXCONF_COMPILER_PREFIX) \
	HOSTCC=/usr/bin/gcc HOSTCXX=/usr/bin/g++ HOSTPKG_CONFIG=/usr/bin/pkg-config \
	KBUILD_BUILD_TIMESTAMP=@$(SOURCE_DATE_EPOCH) KBUILD_BUILD_USER=xxri KBUILD_BUILD_HOST=xxri \
	$(PARALLELMFLAGS)

$(STATEDIR)/xxri-kernel.prepare:
	@$(call targetinfo)
	@rm -rf $(XXRI_KERNEL_BUILD_DIR) && mkdir -p $(XXRI_KERNEL_BUILD_DIR)
	@cp $(XXRI_KERNEL_CFG_DIR)/config-6.12.11-tinycore64 $(XXRI_KERNEL_BUILD_DIR)/.config
	@cd $(XXRI_KERNEL_BUILD_DIR) && KCONFIG_CONFIG=.config \
		$(XXRI_KERNEL_DIR)/scripts/kconfig/merge_config.sh -m -O $(XXRI_KERNEL_BUILD_DIR) \
		.config $(XXRI_KERNEL_CFG_DIR)/sof-x86_64.config >/dev/null
	@$(XXRI_KERNEL_MAKE) olddefconfig
	@$(call touch)

$(STATEDIR)/xxri-kernel.compile:
	@$(call targetinfo)
	@$(XXRI_KERNEL_MAKE) bzImage modules
	@$(XXRI_HOST_PYTHON) $(PTXDIST_WORKSPACE)/scripts/xxri-kabi-check.py \
		$(XXRI_KERNEL_CFG_DIR)/config-6.12.11-tinycore64 $(XXRI_KERNEL_BUILD_DIR)/.config \
		$(XXRI_KERNEL_CFG_DIR)/sof-x86_64.config $(XXRI_KERNEL_CFG_DIR)/Module.symvers-6.12.11-tinycore64.gz \
		$(XXRI_KERNEL_BUILD_DIR)/Module.symvers
	@$(call touch)

$(STATEDIR)/xxri-kernel.install:
	@$(call targetinfo)
	@mkdir -p $(IMAGEDIR) && install -m 644 $(XXRI_KERNEL_BUILD_DIR)/arch/x86/boot/bzImage $(XXRI_KERNEL_IMAGE)
#	# the modules xxri-sound.tcz carries (scripts/xxri-sound-modules.py),
#	# gzip'ed like Tiny Core's
	@rm -rf $(XXRI_KERNEL_MODS_DIR) $(XXRI_KERNEL_MODS_DIR).all
	@$(XXRI_KERNEL_MAKE) modules_install INSTALL_MOD_PATH=$(XXRI_KERNEL_MODS_DIR).all \
		INSTALL_MOD_STRIP=1 DEPMOD=true >/dev/null
	@$(XXRI_HOST_PYTHON) $(PTXDIST_WORKSPACE)/scripts/xxri-sound-modules.py \
		$(XXRI_KERNEL_MODS_DIR).all/lib/modules/$(XXRI_KERNEL_RELEASE)/kernel \
		$(XXRI_KERNEL_CFG_DIR)/tc-modules-x86_64.list $(XXRI_KERNEL_CFG_DIR)/tc-built-modules-x86_64.list \
		$(XXRI_KERNEL_MODS_DIR)
	@rm -rf $(XXRI_KERNEL_MODS_DIR).all
	@$(call touch)

$(STATEDIR)/xxri-kernel.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-kernel)
	@$(call install_fixup, xxri-kernel,PRIORITY,optional)
	@$(call install_fixup, xxri-kernel,SECTION,base)
	@$(call install_fixup, xxri-kernel,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-kernel,DESCRIPTION,"sound modules with SOF and SoundWire for 6.12.11-tinycore64")
	@$(call install_tree, xxri-kernel, 0, 0, $(XXRI_KERNEL_MODS_DIR), \
		/usr/local/lib/modules/$(XXRI_KERNEL_RELEASE)/kernel, n)
	@$(call install_finish, xxri-kernel)
	@$(call touch)

else
XXRI_KERNEL_VERSION	:= 6.12.11-tinycore-16.0
XXRI_KERNEL_SHA256	:= 29638bdbd3469f6f14813bf42bdb072884efccef92fd6101ed60c238a7bab306
XXRI_KERNEL		:= xxri-kernel-$(XXRI_KERNEL_VERSION)
XXRI_KERNEL_URL		:= http://tinycorelinux.net/16.x/x86/archive/16.0/distribution_files/vmlinuz
XXRI_KERNEL_SOURCE	:= $(SRCDIR)/vmlinuz-$(XXRI_KERNEL_VERSION)

# get: PTXdist's own download + sha256 check (upstream publishes md5 4cd98262...)
$(STATEDIR)/xxri-kernel.extract $(STATEDIR)/xxri-kernel.prepare \
$(STATEDIR)/xxri-kernel.compile $(STATEDIR)/xxri-kernel.targetinstall:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-kernel.install:
	@$(call targetinfo)
	@mkdir -p $(IMAGEDIR) && install -m 644 $(XXRI_KERNEL_SOURCE) $(XXRI_KERNEL_IMAGE)
	@$(call touch)
endif
