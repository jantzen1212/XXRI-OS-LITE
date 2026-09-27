# -*-makefile-*-
#
# libglvnd 1.7.0: the three sonames XXRI adds for AppImages
#
PACKAGES-$(PTXCONF_XXRI_GLVND) += xxri-glvnd

XXRI_GLVND_VERSION	:= 1.7.0
XXRI_GLVND_SHA256	:= 073e7292788d4d3eeb45ea6c7bdcce9bfdb3b3eef8d7dbd47f2f30dce046ef98
XXRI_GLVND		:= libglvnd-$(XXRI_GLVND_VERSION)
XXRI_GLVND_SUFFIX	:= tar.gz
# the upstream release as Debian imported it (GitLab's archive/ tarballs are
# generated on demand and have changed bytes before)
XXRI_GLVND_URL		:= https://deb.debian.org/debian/pool/main/libg/libglvnd/libglvnd_$(XXRI_GLVND_VERSION).orig.$(XXRI_GLVND_SUFFIX)
XXRI_GLVND_SOURCE	:= $(SRCDIR)/libglvnd_$(XXRI_GLVND_VERSION).orig.$(XXRI_GLVND_SUFFIX)
XXRI_GLVND_DIR		:= $(BUILDDIR)/$(XXRI_GLVND)
XXRI_GLVND_LICENSE	:= MIT-like (libglvnd README.md)
XXRI_GLVND_LICENSE_FILES := file://README.md;md5=89e4aa253a4fb8677bd414627c885260

XXRI_GLVND_CFLAGS	:= $(XXRI_OS_CFLAGS)
XXRI_GLVND_CONF_TOOL	:= meson
XXRI_GLVND_CONF_OPT	:= $(CROSS_MESON_USR) -Dprefix=/usr/local \
	-Dx11=disabled -Dglx=disabled -Degl=false -Dhgl=false \
	-Dgles1=false -Dgles2=true -Dheaders=false

$(STATEDIR)/xxri-glvnd.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-glvnd)
	@$(call install_fixup, xxri-glvnd,PRIORITY,optional)
	@$(call install_fixup, xxri-glvnd,SECTION,base)
	@$(call install_fixup, xxri-glvnd,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-glvnd,DESCRIPTION,"GL vendor-neutral dispatch (AppImage runtime)")
	@$(foreach l, libGLdispatch libOpenGL libGLESv2, \
		$(call install_lib, xxri-glvnd, 0, 0, 0644, $(l), /usr/local)$(ptx/nl))
	@$(call install_finish, xxri-glvnd)
	@$(call touch)

# vim: syntax=make
