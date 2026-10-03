# -*-makefile-*-
#
# xxri-wallpaper - built from the repository's src/xxri-wallpaper
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_WALLPAPER) += xxri-wallpaper

#
# Paths and names
#
XXRI_WALLPAPER_VERSION	:= $(XXRI_OS_VERSION)
XXRI_WALLPAPER		:= xxri-wallpaper-$(XXRI_WALLPAPER_VERSION)
XXRI_WALLPAPER_URL	:= file://$(XXRI_SRC)/xxri-wallpaper
XXRI_WALLPAPER_DIR	:= $(BUILDDIR)/$(XXRI_WALLPAPER)
XXRI_WALLPAPER_BUILD_DIR	:= $(XXRI_WALLPAPER_DIR)-build
XXRI_WALLPAPER_LICENSE	:= $(XXRI_LICENSE)
XXRI_WALLPAPER_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_WALLPAPER_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_WALLPAPER_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-wallpaper.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_WALLPAPER_BUILD_DIR) && mkdir -p $(XXRI_WALLPAPER_BUILD_DIR)
	@$(call world/execute, XXRI_WALLPAPER, $(CROSS_CC) $(XXRI_OS_CFLAGS) -Wall -Wextra -std=gnu99 \
		$$($(XXRI_PKG_CONFIG) --cflags cairo-xlib xrandr x11 libjpeg) \
		$(XXRI_WALLPAPER_DIR)/xxri-wallpaper.c -o $(XXRI_WALLPAPER_BUILD_DIR)/xxri-wallpaper \
		$(XXRI_TC_LDFLAGS) $$($(XXRI_PKG_CONFIG) --libs cairo-xlib cairo xrandr x11 libjpeg))
	@$(call touch)

$(STATEDIR)/xxri-wallpaper.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-wallpaper.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-wallpaper)
	@$(call install_fixup, xxri-wallpaper,PRIORITY,optional)
	@$(call install_fixup, xxri-wallpaper,SECTION,base)
	@$(call install_fixup, xxri-wallpaper,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-wallpaper,DESCRIPTION,"desktop background renderer")

	@$(call install_copy, xxri-wallpaper, 0, 0, 0755, $(XXRI_WALLPAPER_BUILD_DIR)/xxri-wallpaper, /usr/local/bin/xxri-wallpaper)

	@$(call install_finish, xxri-wallpaper)

	@$(call touch)

# vim: syntax=make
