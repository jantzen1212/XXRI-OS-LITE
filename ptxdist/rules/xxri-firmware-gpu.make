# -*-makefile-*-
#
# xxri-firmware-gpu - a reviewed subset of Tiny Core's GPU firmware packages
# (config/firmware/gpu.list).  The kernel's direct loader does not search
# /usr/local/lib/firmware; Tiny Core's udev helper (lib/udev/firmware.sh)
# serves requests from there through the sysfs fallback.
#
PACKAGES-$(PTXCONF_XXRI_FIRMWARE_GPU) += xxri-firmware-gpu

XXRI_FIRMWARE_GPU_VERSION	:= 20240220
XXRI_FIRMWARE_GPU_SHA256	:= e7d84338ab0fb42ea0a1cea229ece2bd972a4eeec322980f0dd07b84859d1eac
XXRI_FIRMWARE_GPU		:= firmware-radeon
XXRI_FIRMWARE_GPU_SUFFIX	:= tcz
XXRI_FIRMWARE_GPU_URL		:= http://repo.tinycorelinux.net/16.x/x86/tcz/firmware-radeon.tcz
XXRI_FIRMWARE_GPU_SOURCE	:= $(SRCDIR)/firmware-radeon.tcz
XXRI_FIRMWARE_GPU_DIR		:= $(BUILDDIR)/xxri-firmware-gpu-$(XXRI_FIRMWARE_GPU_VERSION)
XXRI_FIRMWARE_GPU_LICENSE	:= LicenseRef-radeon-firmware AND LicenseRef-i915-firmware
XXRI_FIRMWARE_GPU_DEVPKG	:= NO

XXRI_FIRMWARE_GPU_I915_VERSION	:= 20240220
XXRI_FIRMWARE_GPU_I915_SHA256	:= 0f78164fb1e32fa83677541859b07bc7787df5f966626034492be526179edbf7
XXRI_FIRMWARE_GPU_I915		:= firmware-i915
XXRI_FIRMWARE_GPU_I915_SUFFIX	:= tcz
XXRI_FIRMWARE_GPU_I915_URL	:= http://repo.tinycorelinux.net/16.x/x86/tcz/firmware-i915.tcz
XXRI_FIRMWARE_GPU_I915_SOURCE	:= $(SRCDIR)/firmware-i915.tcz
XXRI_FIRMWARE_GPU_I915_DIR	:= $(XXRI_FIRMWARE_GPU_DIR)
XXRI_FIRMWARE_GPU_PARTS		+= XXRI_FIRMWARE_GPU_I915

XXRI_FIRMWARE_GPU_LIST		:= $(PTXDIST_WORKSPACE)/config/firmware/gpu.list
XXRI_FIRMWARE_GPU_FILES		:= $(shell grep -v '^\#' $(XXRI_FIRMWARE_GPU_LIST) 2>/dev/null)

# The sources are squashfs images: unpacked in prepare, where the host
# unsquashfs is guaranteed to exist (host tools come before extract.post).
$(STATEDIR)/xxri-firmware-gpu.extract:
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_GPU_DIR) && mkdir -p $(XXRI_FIRMWARE_GPU_DIR)
	@$(call touch)

$(STATEDIR)/xxri-firmware-gpu.prepare:
	@$(call targetinfo)
	@for s in $(XXRI_FIRMWARE_GPU_SOURCE) $(XXRI_FIRMWARE_GPU_I915_SOURCE); do \
		PATH=$(PTXDIST_SYSROOT_HOST)/usr/bin:$(PTXDIST_SYSROOT_HOST)/bin:$$PATH \
			unsquashfs -q -n -f -d $(XXRI_FIRMWARE_GPU_DIR) $$s >/dev/null || exit 1; \
	done
	@$(call touch)

$(STATEDIR)/xxri-firmware-gpu.compile $(STATEDIR)/xxri-firmware-gpu.install:
	@$(call targetinfo)
	@$(call touch)

# the list decides what is installed: a change re-runs the packaging
$(STATEDIR)/xxri-firmware-gpu.targetinstall: $(XXRI_FIRMWARE_GPU_LIST)
	@$(call targetinfo)
	@$(call install_init, xxri-firmware-gpu)
	@$(call install_fixup, xxri-firmware-gpu,PRIORITY,optional)
	@$(call install_fixup, xxri-firmware-gpu,SECTION,base)
	@$(call install_fixup, xxri-firmware-gpu,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-firmware-gpu,DESCRIPTION,"firmware for legacy Radeon and Intel Gen9 GPUs")
	@$(foreach f, $(XXRI_FIRMWARE_GPU_FILES), \
		$(call install_copy, xxri-firmware-gpu, 0, 0, 0644, \
			$(XXRI_FIRMWARE_GPU_DIR)/usr/local/lib/firmware/$(f), /usr/local/lib/firmware/$(f), n)$(ptx/nl))
#	# the redistribution terms travel with the files
	@$(call install_copy, xxri-firmware-gpu, 0, 0, 0644, \
		$(XXRI_FIRMWARE_GPU_DIR)/usr/local/share/doc/firmware-radeon/LICENSE.radeon, \
		/usr/local/share/doc/xxri-firmware-gpu/LICENSE.radeon)
	@$(call install_copy, xxri-firmware-gpu, 0, 0, 0644, \
		$(XXRI_FIRMWARE_GPU_DIR)/usr/local/share/doc/firmware-i915/LICENSE.i915, \
		/usr/local/share/doc/xxri-firmware-gpu/LICENSE.i915)
	@$(call install_copy, xxri-firmware-gpu, 0, 0, 0644, $(XXRI_FIRMWARE_GPU_LIST), \
		/usr/local/share/doc/xxri-firmware-gpu/gpu.list)
	@$(call install_finish, xxri-firmware-gpu)
	@$(call touch)

# vim: syntax=make
