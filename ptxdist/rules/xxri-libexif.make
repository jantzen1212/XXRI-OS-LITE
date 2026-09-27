# -*-makefile-*-
#
# libexif 0.6.21 for XXRI File
#
PACKAGES-$(PTXCONF_XXRI_LIBEXIF) += xxri-libexif

XXRI_LIBEXIF_VERSION	:= 0.6.21
XXRI_LIBEXIF_SHA256	:= 16cdaeb62eb3e6dfab2435f7d7bccd2f37438d21c5218ec4e58efa9157d4d41a
XXRI_LIBEXIF		:= libexif-$(XXRI_LIBEXIF_VERSION)
XXRI_LIBEXIF_SUFFIX	:= tar.bz2
XXRI_LIBEXIF_URL	:= https://downloads.sourceforge.net/project/libexif/libexif/$(XXRI_LIBEXIF_VERSION)/$(XXRI_LIBEXIF).$(XXRI_LIBEXIF_SUFFIX)
XXRI_LIBEXIF_SOURCE	:= $(SRCDIR)/$(XXRI_LIBEXIF).$(XXRI_LIBEXIF_SUFFIX)
XXRI_LIBEXIF_DIR	:= $(BUILDDIR)/$(XXRI_LIBEXIF)
XXRI_LIBEXIF_LICENSE	:= LGPL-2.0-or-later
XXRI_LIBEXIF_LICENSE_FILES := file://COPYING;md5=243b725d71bb5df4a1e5920b344b86ad

XXRI_LIBEXIF_CFLAGS	:= $(XXRI_OS_CFLAGS)
XXRI_LIBEXIF_CONF_TOOL	:= autoconf
XXRI_LIBEXIF_CONF_OPT	:= $(XXRI_AUTOCONF_LOCAL) --disable-docs --disable-nls

$(STATEDIR)/xxri-libexif.install.post:
	@$(call targetinfo)
	@$(call world/install.post, XXRI_LIBEXIF)
	$(call xxri/pkgconfig-local, XXRI_LIBEXIF, xxri-libexif)
	@$(call touch)

$(STATEDIR)/xxri-libexif.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-libexif)
	@$(call install_fixup, xxri-libexif,PRIORITY,optional)
	@$(call install_fixup, xxri-libexif,SECTION,base)
	@$(call install_fixup, xxri-libexif,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-libexif,DESCRIPTION,"EXIF library")
	@$(call install_lib, xxri-libexif, 0, 0, 0644, libexif, /usr/local)
	@$(call install_finish, xxri-libexif)
	@$(call touch)
