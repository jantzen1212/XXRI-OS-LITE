# -*-makefile-*-
#
# libglapi.so.0 from Mesa 24.3.4, nothing else
#
PACKAGES-$(PTXCONF_XXRI_GLAPI) += xxri-glapi

XXRI_GLAPI_VERSION	:= 24.3.4
XXRI_GLAPI_SHA256	:= e641ae27191d387599219694560d221b7feaa91c900bcec46bf444218ed66025
XXRI_GLAPI		:= mesa-$(XXRI_GLAPI_VERSION)
XXRI_GLAPI_SUFFIX	:= tar.xz
XXRI_GLAPI_URL		:= https://archive.mesa3d.org/$(XXRI_GLAPI).$(XXRI_GLAPI_SUFFIX)
XXRI_GLAPI_SOURCE	:= $(SRCDIR)/$(XXRI_GLAPI).$(XXRI_GLAPI_SUFFIX)
XXRI_GLAPI_DIR		:= $(BUILDDIR)/xxri-glapi-$(XXRI_GLAPI_VERSION)
XXRI_GLAPI_LICENSE	:= MIT

XXRI_GLAPI_CFLAGS	:= $(XXRI_OS_CFLAGS)
XXRI_GLAPI_CONF_TOOL	:= meson
# Mesa builds its shared glapi only when some driver is configured: softpipe
# (no LLVM) satisfies that, and only the libglapi target is compiled.  sse2
# defaults to true on 32-bit x86 and would add -msse2.
XXRI_GLAPI_CONF_OPT	:= $(CROSS_MESON_USR) -Dprefix=/usr/local \
	-Dshared-glapi=enabled -Dopengl=true -Dgles1=disabled -Dgles2=disabled \
	-Dgallium-drivers=softpipe -Dvulkan-drivers= -Dplatforms= \
	-Dglx=disabled -Degl=disabled -Dgbm=disabled -Dglvnd=disabled \
	-Dllvm=disabled -Dsse2=false -Dxmlconfig=disabled -Dexpat=disabled \
	-Dzstd=disabled -Dzlib=disabled -Dshader-cache=disabled -Dlibunwind=disabled \
	-Dvalgrind=disabled -Dlmsensors=disabled -Dbuild-tests=false -Dtools= \
	-Dvideo-codecs= -Dgallium-va=disabled -Dgallium-vdpau=disabled \
	-Dgallium-xa=disabled -Dgallium-nine=false -Dgallium-opencl=disabled \
	-Dgallium-rusticl=false -Dosmesa=false -Dteflon=false -Dperfetto=false \
	-Dhtml-docs=disabled -Dselinux=false

# only libglapi is wanted: build that target, install it by hand
$(STATEDIR)/xxri-glapi.compile:
	@$(call targetinfo)
	@$(call world/execute, XXRI_GLAPI, ninja -C $(XXRI_GLAPI_DIR)-build src/mapi/shared-glapi/libglapi.so.0.0.0)
	@$(call touch)

$(STATEDIR)/xxri-glapi.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-glapi.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-glapi)
	@$(call install_fixup, xxri-glapi,PRIORITY,optional)
	@$(call install_fixup, xxri-glapi,SECTION,base)
	@$(call install_fixup, xxri-glapi,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-glapi,DESCRIPTION,"Mesa shared glapi (AppImage runtime)")
	@$(call install_copy, xxri-glapi, 0, 0, 0644, \
		$(XXRI_GLAPI_DIR)-build/src/mapi/shared-glapi/libglapi.so.0.0.0, /usr/local/lib/libglapi.so.0.0.0)
	@$(call install_link, xxri-glapi, libglapi.so.0.0.0, /usr/local/lib/libglapi.so.0)
	@$(call install_finish, xxri-glapi)
	@$(call touch)

# vim: syntax=make
