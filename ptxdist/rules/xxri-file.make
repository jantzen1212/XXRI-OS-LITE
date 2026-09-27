# -*-makefile-*-
#
# XXRI File - src/xxri-file/pcmanfm, a GTK 3 fork of PCManFM
#
PACKAGES-$(PTXCONF_XXRI_FILE) += xxri-file

XXRI_FILE_VERSION	:= $(XXRI_OS_VERSION)
XXRI_FILE		:= xxri-file-$(XXRI_FILE_VERSION)
XXRI_FILE_URL		:= file://$(XXRI_SRC)/xxri-file
XXRI_FILE_DIR		:= $(BUILDDIR)/$(XXRI_FILE)
XXRI_FILE_LICENSE	:= GPL-2.0-or-later
XXRI_FILE_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_FILE_LICENSE_FILES	:= file://pcmanfm/COPYING;md5=59530bdf33659b29e73d4adb9f9f6552

# a copy of the repository's tree, whose build system is generated here
# (intltoolize + autoreconf with PTXdist's autotools), then built out of tree
XXRI_FILE_SUBDIR	:= pcmanfm
XXRI_FILE_BUILD_OOT	:= YES
XXRI_FILE_CFLAGS	:= $(XXRI_OS_CFLAGS) -fpermissive $(XXRI_OLD_C_FLAGS)
XXRI_FILE_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_FILE_CONF_TOOL	:= autoconf
XXRI_FILE_CONF_OPT	:= $(XXRI_AUTOCONF_LOCAL) --with-gtk=3 --disable-nls

$(STATEDIR)/xxri-file.extract:
	@$(call targetinfo)
	@$(call clean, $(XXRI_FILE_DIR))
	@cp -a $(XXRI_SRC)/xxri-file $(XXRI_FILE_DIR)
	@$(call touch)

# the host autotools are only guaranteed from prepare on
$(STATEDIR)/xxri-file.prepare:
	@$(call targetinfo)
	@cd $(XXRI_FILE_DIR)/pcmanfm && intltoolize --force --automake >/dev/null && \
		ACLOCAL_PATH=$(PTXDIST_SYSROOT_TARGET)/usr/local/share/aclocal autoreconf -fi
	@$(call world/prepare, XXRI_FILE)
	@$(call touch)

# nothing to install into the sysroot
$(STATEDIR)/xxri-file.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-file.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-file)
	@$(call install_fixup, xxri-file,PRIORITY,optional)
	@$(call install_fixup, xxri-file,SECTION,base)
	@$(call install_fixup, xxri-file,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-file,DESCRIPTION,"XXRI File")
	@$(call install_copy, xxri-file, 0, 0, 0755, $(XXRI_FILE_DIR)-build/src/pcmanfm, /usr/local/bin/xxri-file)
	@$(foreach u, about autorun connect desktop-pref pref, \
		$(call install_copy, xxri-file, 0, 0, 0644, $(XXRI_FILE_DIR)-build/data/ui/$(u).ui, \
			/usr/local/share/xxri-file/ui/$(u).ui)$(ptx/nl))
	@$(call install_tree, xxri-file, 0, 0, $(XXRI_FILE_DIR)/root, /)
	@$(call install_finish, xxri-file)
	@$(call touch)
