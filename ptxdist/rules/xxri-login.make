# -*-makefile-*-
#
# xxri-login - built from the repository's src/xxri-login
#

#
# We provide this package
#
PACKAGES-$(PTXCONF_XXRI_LOGIN) += xxri-login

#
# Paths and names
#
XXRI_LOGIN_VERSION	:= $(XXRI_OS_VERSION)
XXRI_LOGIN		:= xxri-login-$(XXRI_LOGIN_VERSION)
XXRI_LOGIN_URL		:= file://$(XXRI_SRC)/xxri-login
XXRI_LOGIN_DIR		:= $(BUILDDIR)/$(XXRI_LOGIN)
XXRI_LOGIN_BUILD_DIR	:= $(XXRI_LOGIN_DIR)-build
XXRI_LOGIN_LICENSE	:= $(XXRI_LICENSE)
XXRI_LOGIN_CPPFLAGS	:= $(XXRI_TC_CPPFLAGS)
XXRI_LOGIN_LDFLAGS	:= $(XXRI_TC_LDFLAGS)
XXRI_LOGIN_CONF_TOOL	:= NO

# ----------------------------------------------------------------------------
# Compile
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-login.compile:
	@$(call targetinfo)
	@rm -rf $(XXRI_LOGIN_BUILD_DIR) && mkdir -p $(XXRI_LOGIN_BUILD_DIR)
	@$(call world/execute, XXRI_LOGIN, $(CROSS_CC) $(XXRI_OS_CFLAGS) -w $(XXRI_GTK3_CFLAGS) $(XXRI_LOGIN_DIR)/xxri-login.c -o $(XXRI_LOGIN_BUILD_DIR)/xxri-login $(XXRI_TC_LDFLAGS) $(XXRI_GTK3_LIBS) -lX11 -lcrypt -lm -mtls-dialect=gnu -static-libgcc)
	@$(call xxri/mo-compile, XXRI_LOGIN, xxri-login)
	@$(call touch)

$(STATEDIR)/xxri-login.install:
	@$(call targetinfo)
	@$(call touch)

# ----------------------------------------------------------------------------
# Target-Install
# ----------------------------------------------------------------------------

$(STATEDIR)/xxri-login.targetinstall:
	@$(call targetinfo)

	@$(call install_init, xxri-login)
	@$(call install_fixup, xxri-login,PRIORITY,optional)
	@$(call install_fixup, xxri-login,SECTION,base)
	@$(call install_fixup, xxri-login,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-login,DESCRIPTION,"login screen")

	@$(call install_copy, xxri-login, 0, 0, 0755, $(XXRI_LOGIN_BUILD_DIR)/xxri-login, /usr/local/bin/xxri-login)
	@$(call install_tree, xxri-login, 0, 0, $(XXRI_LOGIN_DIR)/root, /)

	@$(call xxri/mo-install, xxri-login, XXRI_LOGIN)

	@$(call install_finish, xxri-login)

	@$(call touch)

# vim: syntax=make
