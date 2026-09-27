# -*-makefile-*-
#
# host-xxri-grub - upstream GNU GRUB 2.14, unpatched, for the XXRI boot
# targets: i386-pc (BIOS), i386-efi (IA32 UEFI) and x86_64-efi (only for the
# opt-in experiment).  PTXdist's own "grub" rule is GRUB Legacy 0.97.
# Same configuration as ../tools/build-grub.sh (Build A).
#
HOST_PACKAGES-$(PTXCONF_HOST_XXRI_GRUB) += host-xxri-grub

HOST_XXRI_GRUB_VERSION		:= 2.14
HOST_XXRI_GRUB_SHA256		:= bc8d3c73535b8838d8c8e2654d73edc4e6ae8c8acdb45d5df5dc9a1547446d43
HOST_XXRI_GRUB			:= grub-$(HOST_XXRI_GRUB_VERSION)
HOST_XXRI_GRUB_SUFFIX		:= tar.xz
HOST_XXRI_GRUB_URL		:= $(call ptx/mirror, GNU, grub/$(HOST_XXRI_GRUB).$(HOST_XXRI_GRUB_SUFFIX))
HOST_XXRI_GRUB_SOURCE		:= $(SRCDIR)/$(HOST_XXRI_GRUB).$(HOST_XXRI_GRUB_SUFFIX)
HOST_XXRI_GRUB_DIR		:= $(HOST_BUILDDIR)/$(HOST_XXRI_GRUB)
HOST_XXRI_GRUB_LICENSE		:= GPL-3.0-or-later
HOST_XXRI_GRUB_LICENSE_FILES	:= file://COPYING;md5=d32239bcb673463ab874e80d47fae504
HOST_XXRI_GRUB_PLATFORMS	:= i386-pc i386-efi x86_64-efi
# the tree layout build-iso.sh expects: <prefix>/<platform>/usr/{bin,lib/grub/<platform>}
HOST_XXRI_GRUB_PREFIX		:= $(PTXDIST_SYSROOT_HOST)/lib/xxri-grub

# GNU ld >= 2.44 accepts -Wl,--image-base, which GRUB 2.14's configure then
# prefers over -Ttext and which misplaces i386-pc kernel.img; see build-grub.sh.
HOST_XXRI_GRUB_CONF_OPT		:= \
	--prefix=/usr --disable-werror --disable-nls --disable-grub-mkfont \
	--disable-grub-themes --disable-grub-mount --disable-device-mapper \
	--disable-libzfs --disable-liblzma \
	ax_cv_check_ldflags___Wl___image_base_0x400000=no

# Same relative source/object layout as ../tools/build-grub.sh (<p>/src and
# <p>/obj): GRUB embeds __FILE__ paths, so this is what makes the two build
# systems produce byte-identical modules and core images.
$(STATEDIR)/host-xxri-grub.prepare:
	@$(call targetinfo)
	@for p in $(HOST_XXRI_GRUB_PLATFORMS); do \
		rm -rf $(HOST_XXRI_GRUB_DIR)-$$p && mkdir -p $(HOST_XXRI_GRUB_DIR)-$$p/obj && \
		ln -s $(HOST_XXRI_GRUB_DIR) $(HOST_XXRI_GRUB_DIR)-$$p/src && \
		( cd $(HOST_XXRI_GRUB_DIR)-$$p/obj && $(HOST_ENV) ../src/configure --quiet \
			--target=$${p%%-*} --with-platform=$${p#*-} $(HOST_XXRI_GRUB_CONF_OPT) ) || exit 1; \
	done
	@$(call touch)

$(STATEDIR)/host-xxri-grub.compile:
	@$(call targetinfo)
	@for p in $(HOST_XXRI_GRUB_PLATFORMS); do \
		$(MAKE) -C $(HOST_XXRI_GRUB_DIR)-$$p/obj $(PARALLELMFLAGS) || exit 1; \
	done
	@$(call touch)

$(STATEDIR)/host-xxri-grub.install:
	@$(call targetinfo)
	@for p in $(HOST_XXRI_GRUB_PLATFORMS); do \
		rm -rf $(HOST_XXRI_GRUB_PREFIX)/$$p && \
		$(MAKE) -C $(HOST_XXRI_GRUB_DIR)-$$p/obj install DESTDIR=$(HOST_XXRI_GRUB_PREFIX)/$$p || exit 1; \
	done
	@$(call touch)
