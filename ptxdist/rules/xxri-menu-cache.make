# -*-makefile-*-
#
# menu-cache 1.1.0 for libfm
#
PACKAGES-$(PTXCONF_XXRI_MENU_CACHE) += xxri-menu-cache

XXRI_MENU_CACHE_VERSION	:= 1.1.0
XXRI_MENU_CACHE_SHA256	:= ed02eb459dcb398f69b9fa5bf4dd813020405afc84331115469cdf7be9273ec7
XXRI_MENU_CACHE		:= menu-cache-$(XXRI_MENU_CACHE_VERSION)
XXRI_MENU_CACHE_SUFFIX	:= tar.xz
XXRI_MENU_CACHE_URL	:= https://downloads.sourceforge.net/project/lxde/menu-cache/1.1/$(XXRI_MENU_CACHE).$(XXRI_MENU_CACHE_SUFFIX)
XXRI_MENU_CACHE_SOURCE	:= $(SRCDIR)/$(XXRI_MENU_CACHE).$(XXRI_MENU_CACHE_SUFFIX)
XXRI_MENU_CACHE_DIR	:= $(BUILDDIR)/$(XXRI_MENU_CACHE)
XXRI_MENU_CACHE_LICENSE	:= LGPL-2.1-or-later AND GPL-2.0-or-later
XXRI_MENU_CACHE_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)

XXRI_MENU_CACHE_CFLAGS	:= $(XXRI_OS_CFLAGS) $(XXRI_OLD_C_FLAGS) -fcommon
XXRI_MENU_CACHE_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_MENU_CACHE_CONF_TOOL := autoconf
XXRI_MENU_CACHE_CONF_OPT := $(XXRI_AUTOCONF_LOCAL) --disable-gtk-doc

$(STATEDIR)/xxri-menu-cache.install.post:
	@$(call targetinfo)
	@$(call world/install.post, XXRI_MENU_CACHE)
	$(call xxri/pkgconfig-local, XXRI_MENU_CACHE, xxri-menu-cache)
	@$(call touch)

$(STATEDIR)/xxri-menu-cache.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-menu-cache)
	@$(call install_fixup, xxri-menu-cache,PRIORITY,optional)
	@$(call install_fixup, xxri-menu-cache,SECTION,base)
	@$(call install_fixup, xxri-menu-cache,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-menu-cache,DESCRIPTION,"menu cache library")
	@$(call install_lib, xxri-menu-cache, 0, 0, 0644, libmenu-cache, /usr/local)
	@$(call install_finish, xxri-menu-cache)
	@$(call touch)
