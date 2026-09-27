# -*-makefile-*-
PACKAGES-$(PTXCONF_XXRI_GRUB_TOOLS) += xxri-grub-tools

XXRI_GRUB_TOOLS_VERSION		:= 2.14
XXRI_GRUB_TOOLS_SHA256		:= bc8d3c73535b8838d8c8e2654d73edc4e6ae8c8acdb45d5df5dc9a1547446d43
XXRI_GRUB_TOOLS			:= grub-$(XXRI_GRUB_TOOLS_VERSION)
XXRI_GRUB_TOOLS_SUFFIX		:= tar.xz
XXRI_GRUB_TOOLS_URL		:= $(call ptx/mirror, GNU, grub/$(XXRI_GRUB_TOOLS).$(XXRI_GRUB_TOOLS_SUFFIX))
XXRI_GRUB_TOOLS_SOURCE		:= $(SRCDIR)/$(XXRI_GRUB_TOOLS).$(XXRI_GRUB_TOOLS_SUFFIX)
XXRI_GRUB_TOOLS_DIR		:= $(BUILDDIR)/xxri-grub-tools-$(XXRI_GRUB_TOOLS_VERSION)
XXRI_GRUB_TOOLS_LICENSE		:= GPL-3.0-or-later
XXRI_GRUB_TOOLS_LICENSE_FILES	:= file://COPYING;md5=d32239bcb673463ab874e80d47fae504
XXRI_GRUB_TOOLS_OUT		:= $(XXRI_GRUB_TOOLS_DIR)-out/sbin
# targetinstall takes files from the build tree and from host-xxri-grub's
# module trees, which a prebuilt development package could not provide
XXRI_GRUB_TOOLS_DEVPKG		:= NO

XXRI_GRUB_TOOLS_CONF_TOOL	:= autoconf
# The firmware-side code of this build also comes from the cross compiler;
# only the five utilities are shipped.
XXRI_GRUB_TOOLS_CONF_ENV	:= $(CROSS_ENV) \
	TARGET_CC=$(CROSS_CC) TARGET_OBJCOPY=$(CROSS_OBJCOPY) TARGET_STRIP=$(CROSS_STRIP) \
	TARGET_NM=$(CROSS_NM) TARGET_RANLIB=$(CROSS_RANLIB) \
	ax_cv_check_ldflags___Wl___image_base_0x400000=no
XXRI_GRUB_TOOLS_CONF_OPT	:= \
	--host=$(PTXCONF_GNU_TARGET) --build=$(GNU_HOST) \
	--target=i386 --with-platform=pc --prefix=/usr/local \
	--disable-werror --disable-nls --disable-grub-mkfont --disable-grub-themes \
	--disable-grub-mount --disable-device-mapper --disable-libzfs --disable-liblzma

$(STATEDIR)/xxri-grub-tools.install:
	@$(call targetinfo)
	@$(call world/install, XXRI_GRUB_TOOLS)
	@rm -rf $(XXRI_GRUB_TOOLS_OUT) && mkdir -p $(XXRI_GRUB_TOOLS_OUT)
	@for t in grub-install grub-mkimage grub-bios-setup grub-probe grub-editenv; do \
		f=$$(find $(XXRI_GRUB_TOOLS_PKGDIR)/usr/local -name $$t -type f | head -1); \
		$(CROSS_STRIP) -o $(XXRI_GRUB_TOOLS_OUT)/$$t $$f || exit 1; \
	done
	@$(call touch)

$(STATEDIR)/xxri-grub-tools.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-grub-tools)
	@$(call install_fixup, xxri-grub-tools,PRIORITY,optional)
	@$(call install_fixup, xxri-grub-tools,SECTION,base)
	@$(call install_fixup, xxri-grub-tools,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-grub-tools,DESCRIPTION,"GRUB 2.14 for the installer")
#	# the i686 utilities built here
	@$(foreach t, grub-install grub-mkimage grub-bios-setup grub-probe grub-editenv, \
		$(call install_copy, xxri-grub-tools, 0, 0, 0755, $(XXRI_GRUB_TOOLS_OUT)/$(t), \
			/usr/local/sbin/$(t), n)$(ptx/nl))
#	# the module trees of the boot images on the medium (host-xxri-grub), so an
#	# installed disk boots the same GRUB bits; *.module/*.exec/gdb_grub are
#	# debug copies for gdb (15 MB of 18)
	@$(foreach p, i386-pc i386-efi, \
		$(call install_glob, xxri-grub-tools, 0, 0, \
			$(HOST_XXRI_GRUB_PREFIX)/$(p)/usr/lib/grub/$(p), /usr/local/lib/grub/$(p), , \
			*.module *.exec */gdb_grub *.py */config.h, n)$(ptx/nl))
	@$(call install_copy, xxri-grub-tools, 0, 0, 0644, $(XXRI_GRUB_TOOLS_DIR)/COPYING, \
		/usr/local/share/doc/xxri-grub/COPYING)
	@$(call install_copy, xxri-grub-tools, 0, 0, 0644, $(PTXDIST_WORKSPACE)/config/xxri-grub/SOURCE.txt, \
		/usr/local/share/doc/xxri-grub/SOURCE.txt)

	@$(call install_finish, xxri-grub-tools)

	@$(call touch)
