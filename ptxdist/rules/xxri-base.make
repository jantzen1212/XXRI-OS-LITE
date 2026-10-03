# -*-makefile-*-
#
# xxri-base - projectroot/ installed over Tiny Core's base.  A file that
# replaces a Tiny Core file keeps Tiny Core's owner and mode in core.gz
# (scripts/xxri-mkinitramfs.py), so only new files need a mode here: git keeps
# 0644/0755, the two Tiny Core-style group-writable files are listed below.
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_BASE) += xxri-base

#
# Paths and names
#
XXRI_BASE_VERSION	:= $(XXRI_OS_VERSION)
XXRI_BASE		:= xxri-base-$(XXRI_BASE_VERSION)
XXRI_BASE_LICENSE	:= $(XXRI_LICENSE)
# realpath: find does not descend a start directory that is a symlink
XXRI_BASE_ROOT		:= $(realpath $(PTXDIST_WORKSPACE)/projectroot)

# files installed with an explicit owner and mode: path mode uid gid
XXRI_BASE_SPECIAL	:= \
	opt/xxri-mirror:0664:0:50 \
	usr/share/doc/xxri/bootOptions.txt:0664:0:0

$(STATEDIR)/xxri-base.get $(STATEDIR)/xxri-base.extract $(STATEDIR)/xxri-base.prepare \
$(STATEDIR)/xxri-base.compile $(STATEDIR)/xxri-base.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

# PTXdist does not watch projectroot/: an edited file re-runs the packaging
# (only core.gz depends on it).  Regular files - a symlink's target may not
# exist on the build host.
$(STATEDIR)/xxri-base.targetinstall: $(shell find $(XXRI_BASE_ROOT) -type f)
	@$(call targetinfo)

	@$(call install_init, xxri-base)
	@$(call install_fixup, xxri-base,PRIORITY,optional)
	@$(call install_fixup, xxri-base,SECTION,base)
	@$(call install_fixup, xxri-base,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-base,DESCRIPTION,"XXRI configuration, scripts and artwork")

	@$(call install_glob, xxri-base, 0, 0, $(XXRI_BASE_ROOT), /, , \
		$(foreach s,$(XXRI_BASE_SPECIAL),*/projectroot/$(word 1,$(subst :, ,$(s)))))
	@$(foreach s,$(XXRI_BASE_SPECIAL), \
		$(call install_copy, xxri-base, $(word 3,$(subst :, ,$(s))), $(word 4,$(subst :, ,$(s))), \
			$(word 2,$(subst :, ,$(s))), $(XXRI_BASE_ROOT)/$(word 1,$(subst :, ,$(s))), \
			/$(word 1,$(subst :, ,$(s))))$(ptx/nl))
#	# system-wide AppImage directory scanned by xxri-app (empty; git keeps no dirs)
	@$(call install_copy, xxri-base, 0, 0, 0755, /opt/appimages)

	@$(call install_finish, xxri-base)

	@$(call touch)

# vim: syntax=make
