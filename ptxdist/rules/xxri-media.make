# -*-makefile-*-
#
# xxri-media - built from the repository's src/xxri-media
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_MEDIA) += xxri-media

#
# Paths and names
#
XXRI_MEDIA_VERSION	:= $(XXRI_OS_VERSION)
XXRI_MEDIA		:= xxri-media-$(XXRI_MEDIA_VERSION)
XXRI_MEDIA_URL		:= file://$(XXRI_SRC)/xxri-media
XXRI_MEDIA_DIR		:= $(BUILDDIR)/$(XXRI_MEDIA)
XXRI_MEDIA_BUILD_DIR	:= $(XXRI_MEDIA_DIR)-build
XXRI_MEDIA_LICENSE	:= $(XXRI_LICENSE)
XXRI_MEDIA_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_MEDIA_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_MEDIA_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-media.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_MEDIA_BUILD_DIR) && mkdir -p $(XXRI_MEDIA_BUILD_DIR)
	@$(call world/execute, XXRI_MEDIA, $(CROSS_CC) $(XXRI_OS_CFLAGS) -w $(XXRI_GTK3_CFLAGS) $(XXRI_MEDIA_DIR)/xxri-media.c $(XXRI_MEDIA_DIR)/player.c -o $(XXRI_MEDIA_BUILD_DIR)/xxri-media $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lX11 -lm -mtls-dialect=gnu -static-libgcc)
	@$(call touch)

$(STATEDIR)/xxri-media.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-media.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-media)
	@$(call install_fixup, xxri-media,PRIORITY,optional)
	@$(call install_fixup, xxri-media,SECTION,base)
	@$(call install_fixup, xxri-media,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-media,DESCRIPTION,"media player")

	@$(call install_copy, xxri-media, 0, 0, 0755, $(XXRI_MEDIA_BUILD_DIR)/xxri-media, /usr/local/bin/xxri-media)
	@$(call install_tree, xxri-media, 0, 0, $(XXRI_MEDIA_DIR)/root, /)

	@$(call install_finish, xxri-media)

	@$(call touch)

# vim: syntax=make
