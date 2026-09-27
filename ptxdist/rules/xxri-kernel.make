# -*-makefile-*-
PACKAGES-$(PTXCONF_XXRI_KERNEL) += xxri-kernel

XXRI_KERNEL_VERSION	:= 6.12.11-tinycore-16.0
XXRI_KERNEL_SHA256	:= 29638bdbd3469f6f14813bf42bdb072884efccef92fd6101ed60c238a7bab306
XXRI_KERNEL		:= xxri-kernel-$(XXRI_KERNEL_VERSION)
XXRI_KERNEL_URL		:= http://tinycorelinux.net/16.x/x86/archive/16.0/distribution_files/vmlinuz
XXRI_KERNEL_SOURCE	:= $(SRCDIR)/vmlinuz-$(XXRI_KERNEL_VERSION)
XXRI_KERNEL_LICENSE	:= GPL-2.0-only
XXRI_KERNEL_IMAGE	:= $(IMAGEDIR)/vmlinuz

# get: PTXdist's own download + sha256 check (upstream publishes md5 4cd98262...)
$(STATEDIR)/xxri-kernel.extract $(STATEDIR)/xxri-kernel.prepare \
$(STATEDIR)/xxri-kernel.compile $(STATEDIR)/xxri-kernel.targetinstall:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-kernel.install:
	@$(call targetinfo)
	@mkdir -p $(IMAGEDIR) && install -m 644 $(XXRI_KERNEL_SOURCE) $(XXRI_KERNEL_IMAGE)
	@$(call touch)
