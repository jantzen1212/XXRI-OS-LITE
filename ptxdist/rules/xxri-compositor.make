# -*-makefile-*-
#
# xxri-compositor - built from the repository's src/xxri-compositor
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_COMPOSITOR) += xxri-compositor

#
# Paths and names
#
XXRI_COMPOSITOR_VERSION	:= $(XXRI_OS_VERSION)
XXRI_COMPOSITOR		:= xxri-compositor-$(XXRI_COMPOSITOR_VERSION)
XXRI_COMPOSITOR_URL		:= file://$(XXRI_SRC)/xxri-compositor
XXRI_COMPOSITOR_DIR		:= $(BUILDDIR)/$(XXRI_COMPOSITOR)
XXRI_COMPOSITOR_BUILD_DIR	:= $(XXRI_COMPOSITOR_DIR)-build
XXRI_COMPOSITOR_LICENSE	:= $(XXRI_LICENSE)
XXRI_COMPOSITOR_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_COMPOSITOR_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_COMPOSITOR_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-compositor.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_COMPOSITOR_BUILD_DIR) && mkdir -p $(XXRI_COMPOSITOR_BUILD_DIR)
	@$(call world/execute, XXRI_COMPOSITOR, $(CROSS_CC) $(XXRI_OS_CFLAGS) -Wall -Wextra -std=gnu99 $(XXRI_COMPOSITOR_DIR)/xxri-compositor.c -o $(XXRI_COMPOSITOR_BUILD_DIR)/xxri-compositor $(XXRI_TC_LDFLAGS) $$($(XXRI_PKG_CONFIG) --libs xcomposite xdamage xfixes xrender xext x11))
	@$(call touch)

$(STATEDIR)/xxri-compositor.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-compositor.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-compositor)
	@$(call install_fixup, xxri-compositor,PRIORITY,optional)
	@$(call install_fixup, xxri-compositor,SECTION,base)
	@$(call install_fixup, xxri-compositor,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-compositor,DESCRIPTION,"translucency compositor")

	@$(call install_copy, xxri-compositor, 0, 0, 0755, $(XXRI_COMPOSITOR_BUILD_DIR)/xxri-compositor, /usr/local/bin/xxri-compositor)

	@$(call install_finish, xxri-compositor)

	@$(call touch)

# vim: syntax=make
