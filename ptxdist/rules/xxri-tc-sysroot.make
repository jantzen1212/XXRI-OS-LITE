# -*-makefile-*-
PACKAGES-$(PTXCONF_XXRI_TC_SYSROOT) += xxri-tc-sysroot

XXRI_TC_SYSROOT_VERSION	:= 16.x
XXRI_TC_SYSROOT		:= xxri-tc-sysroot-$(XXRI_TC_SYSROOT_VERSION)
XXRI_TC_SYSROOT_LICENSE	:= various (Tiny Core 16.x packages)
XXRI_TC_SYSROOT_CACHE	:= $(SRCDIR)/xxri-tc-sysroot
# re-run from prepare whenever the manifest changes
XXRI_TC_SYSROOT_CONFIG	:= $(PTXDIST_WORKSPACE)/config/tc-sysroot.manifest

# install.post too: PTXdist's default would rewrite xxri-tc-sysroot.pkgconfig
# (the module list written by .install) from the empty package directory.
$(STATEDIR)/xxri-tc-sysroot.get $(STATEDIR)/xxri-tc-sysroot.extract \
$(STATEDIR)/xxri-tc-sysroot.prepare $(STATEDIR)/xxri-tc-sysroot.compile \
$(STATEDIR)/xxri-tc-sysroot.install.post $(STATEDIR)/xxri-tc-sysroot.targetinstall:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-tc-sysroot.install:
	@$(call targetinfo)
	@PATH=$(PTXDIST_SYSROOT_HOST)/bin:$$PATH $(PTXDIST_WORKSPACE)/scripts/xxri-tcz-sysroot.sh \
		$(PTXDIST_WORKSPACE)/config/tc-sysroot.manifest $(XXRI_TC_SYSROOT_CACHE) \
		$(PTXDIST_SYSROOT_TARGET) $(STATEDIR)/xxri-tc-sysroot.pkgconfig \
		$(XXRI_TC_BASE_SOURCE)
	@$(call touch)
