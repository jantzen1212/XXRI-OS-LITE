# -*-makefile-*-
#
# xxri-flwm - flwm with the XXRI decorations
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_FLWM) += xxri-flwm

#
# Paths and names
#
XXRI_FLWM_VERSION	:= git-196f61c
XXRI_FLWM_COMMIT	:= 196f61c036ef65d65d5ec4095667f9a2d21822e2
XXRI_FLWM_SHA256	:= 2f07cf554b0e96dddf143dbfcf2995512864be2ea36454c3506ed20e7001e39f
XXRI_FLWM		:= flwm-$(XXRI_FLWM_VERSION)
XXRI_FLWM_SUFFIX	:= tar.gz
XXRI_FLWM_URL		:= https://github.com/tinycorelinux/flwm.git;tag=$(XXRI_FLWM_COMMIT)
XXRI_FLWM_SOURCE	:= $(SRCDIR)/$(XXRI_FLWM).$(XXRI_FLWM_SUFFIX)
XXRI_FLWM_DIR		:= $(BUILDDIR)/$(XXRI_FLWM)
XXRI_FLWM_LICENSE	:= GPL-2.0-or-later

# flwm-stock: Tiny Core's flwm as 16.0 shipped it - the last commit before the
# FLTK 1.4 port (Tiny Core 16 has FLTK 1.3; the XXRI patch guards the 1.4 calls)
XXRI_FLWM_STOCK_VERSION	:= git-0235344
XXRI_FLWM_STOCK_COMMIT	:= 0235344b823bf167c18173c5a9a2cadf9f3c8fa5
XXRI_FLWM_STOCK_SHA256	:= 3c95ad54544c077de6a34cbef9780cf1d537fb7543a1be05b1cf5944b8982a5b
XXRI_FLWM_STOCK		:= flwm-$(XXRI_FLWM_STOCK_VERSION)
XXRI_FLWM_STOCK_SUFFIX	:= tar.gz
XXRI_FLWM_STOCK_URL	:= https://github.com/tinycorelinux/flwm.git;tag=$(XXRI_FLWM_STOCK_COMMIT)
XXRI_FLWM_STOCK_SOURCE	:= $(SRCDIR)/$(XXRI_FLWM_STOCK).$(XXRI_FLWM_STOCK_SUFFIX)
XXRI_FLWM_STOCK_DIR	:= $(XXRI_FLWM_DIR)-stock
XXRI_FLWM_PARTS		+= XXRI_FLWM_STOCK
XXRI_FLWM_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_FLWM_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_FLWM_CONF_TOOL	:= NO

# the flags of Tiny Core's own flwm build (compile_flwm), XXRI's CPU flags
XXRI_FLWM_FLAGS		:= $(XXRI_OS_CFLAGS) -w -fno-exceptions -fno-rtti \
	-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
	-D_THREAD_SAFE -D_REENTRANT -I$(PTXDIST_SYSROOT_TARGET)/usr/local/include

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

XXRI_FLWM_LIBS		:= $(XXRI_TC_LDFLAGS) -lfltk -lXcursor -lXext -lX11 -lm -static-libgcc

$(STATEDIR)/xxri-flwm.compile:
	@$(call targetinfo)
	@$(call world/execute, XXRI_FLWM, \
		$(CROSS_CXX) $(XXRI_FLWM_FLAGS) -DTOPSIDE -DXXRI -I. *.C -o flwm-xxri $(XXRI_FLWM_LIBS))
	@$(call world/execute, XXRI_FLWM, \
		cd $(XXRI_FLWM_DIR)-stock && \
		$(CROSS_CXX) $(XXRI_FLWM_FLAGS) -I. *.C -o flwm-stock $(XXRI_FLWM_LIBS))
	@$(call touch)

$(STATEDIR)/xxri-flwm.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-flwm.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-flwm)
	@$(call install_fixup, xxri-flwm,PRIORITY,optional)
	@$(call install_fixup, xxri-flwm,SECTION,base)
	@$(call install_fixup, xxri-flwm,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-flwm,DESCRIPTION,"XXRI window manager")

	@$(call install_copy, xxri-flwm, 0, 0, 0755, $(XXRI_FLWM_DIR)/flwm-xxri, /usr/local/bin/flwm)
#	# recovery: Tiny Core's flwm from the same source, without the XXRI patch
#	# (flwm.tcz's own /usr/local/bin/flwm is shadowed by the one above)
	@$(call install_copy, xxri-flwm, 0, 0, 0755, $(XXRI_FLWM_DIR)-stock/flwm-stock, /usr/local/bin/flwm-stock)

	@$(call install_finish, xxri-flwm)

	@$(call touch)

# vim: syntax=make
