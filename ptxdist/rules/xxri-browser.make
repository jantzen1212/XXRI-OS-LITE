# -*-makefile-*-
#
# XXRI Browser - src/xxri-browser/browser against xxri-qt5
#
PACKAGES-$(PTXCONF_XXRI_BROWSER) += xxri-browser

XXRI_BROWSER_VERSION	:= $(XXRI_OS_VERSION)
XXRI_BROWSER		:= xxri-browser-$(XXRI_BROWSER_VERSION)
XXRI_BROWSER_URL	:= file://$(XXRI_SRC)/xxri-browser
XXRI_BROWSER_DIR	:= $(BUILDDIR)/$(XXRI_BROWSER)
XXRI_BROWSER_BUILD_DIR	:= $(XXRI_BROWSER_DIR)-build
XXRI_BROWSER_LICENSE	:= BSD-3-Clause AND $(XXRI_LICENSE)
XXRI_BROWSER_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_BROWSER_CONF_TOOL	:= NO
XXRI_BROWSER_CFLAGS	:= $(XXRI_SSE2_CFLAGS)
XXRI_BROWSER_CXXFLAGS	:= $(XXRI_SSE2_CFLAGS)
XXRI_BROWSER_LDFLAGS	:= $(XXRI_TC_LDFLAGS) -Wl,-rpath-link,$(PTXDIST_SYSROOT_TARGET)$(XXRI_BROWSER_HOME)/lib

$(STATEDIR)/xxri-browser.prepare:
	@$(call targetinfo)
	@rm -rf $(XXRI_BROWSER_BUILD_DIR) && mkdir -p $(XXRI_BROWSER_BUILD_DIR)
	@cd $(XXRI_BROWSER_BUILD_DIR) && $(CROSS_ENV_PKG_CONFIG) \
		$(XXRI_QT5_QMAKE) $(XXRI_BROWSER_DIR)/browser/xxri-browser.pro
	@$(call touch)

$(STATEDIR)/xxri-browser.compile:
	@$(call targetinfo)
	@$(call world/execute, XXRI_BROWSER, $(MAKE) -C $(XXRI_BROWSER_BUILD_DIR) $(PARALLELMFLAGS))
	@$(call touch)

$(STATEDIR)/xxri-browser.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-browser.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-browser)
	@$(call install_fixup, xxri-browser,PRIORITY,optional)
	@$(call install_fixup, xxri-browser,SECTION,base)
	@$(call install_fixup, xxri-browser,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-browser,DESCRIPTION,"XXRI Browser")
	@$(call install_copy, xxri-browser, 0, 0, 0755, $(XXRI_BROWSER_BUILD_DIR)/xxri-browser, \
		$(XXRI_BROWSER_HOME)/bin/xxri-browser)
	@$(call install_tree, xxri-browser, 0, 0, $(XXRI_BROWSER_DIR)/root, /)
	@$(call install_finish, xxri-browser)
	@$(call touch)
