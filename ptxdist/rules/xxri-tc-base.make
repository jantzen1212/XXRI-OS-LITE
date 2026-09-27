# -*-makefile-*-
#
# Tiny Core 16.0's core.gz, the base layer of XXRI's initramfs.
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_TC_BASE) += xxri-tc-base

#
# Paths and names
#
XXRI_TC_BASE_VERSION	:= 16.0
XXRI_TC_BASE_SHA256	:= 5d5c282ac7b07120f3a39f35cb935b4d7f0621cca3940fa541de4d96dbc6d060
XXRI_TC_BASE		:= tinycore-core-$(XXRI_TC_BASE_VERSION)
XXRI_TC_BASE_URL	:= http://tinycorelinux.net/16.x/x86/archive/16.0/distribution_files/core.gz
XXRI_TC_BASE_SOURCE	:= $(SRCDIR)/$(XXRI_TC_BASE).gz
XXRI_TC_BASE_LICENSE	:= GPL-2.0-only AND various (Tiny Core Linux 16.0)

# what XXRI deletes from the Tiny Core tree (everything else it replaces or adds)
XXRI_TC_BASE_REMOVE	:= $(PTXDIST_WORKSPACE)/config/tc-base.remove

# ----------------------------------------------------------------------------
# Nothing to build: the archive is used by image-xxri-core as it is
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-tc-base.extract $(STATEDIR)/xxri-tc-base.prepare \
$(STATEDIR)/xxri-tc-base.compile $(STATEDIR)/xxri-tc-base.install \
$(STATEDIR)/xxri-tc-base.targetinstall:
	@$(call targetinfo)
	@$(call touch)

# vim: syntax=make
