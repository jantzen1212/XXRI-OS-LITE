# -*-makefile-*-
#
# host-xxri-xorriso - GNU xorriso (standalone libburnia build), used to
# assemble the ISO.  PTXdist ships no xorriso rule.
#
HOST_PACKAGES-$(PTXCONF_HOST_XXRI_XORRISO) += host-xxri-xorriso

HOST_XXRI_XORRISO_VERSION	:= 1.5.8.pl02
HOST_XXRI_XORRISO_SHA256	:= b1455ecafbf0692ddafe1d71002a96f2ce2d77f4deae602678261ce033f97bc8
HOST_XXRI_XORRISO		:= xorriso-$(HOST_XXRI_XORRISO_VERSION)
HOST_XXRI_XORRISO_SUFFIX	:= tar.gz
HOST_XXRI_XORRISO_URL		:= $(call ptx/mirror, GNU, xorriso/$(HOST_XXRI_XORRISO).$(HOST_XXRI_XORRISO_SUFFIX))
HOST_XXRI_XORRISO_SOURCE	:= $(SRCDIR)/$(HOST_XXRI_XORRISO).$(HOST_XXRI_XORRISO_SUFFIX)
HOST_XXRI_XORRISO_DIR		:= $(HOST_BUILDDIR)/$(HOST_XXRI_XORRISO)
HOST_XXRI_XORRISO_LICENSE	:= GPL-3.0-or-later
HOST_XXRI_XORRISO_LICENSE_FILES	:= file://COPYING;md5=d32239bcb673463ab874e80d47fae504

HOST_XXRI_XORRISO_CONF_TOOL	:= autoconf
HOST_XXRI_XORRISO_CONF_OPT	:= \
	$(HOST_AUTOCONF) \
	--disable-libreadline \
	--disable-libedit \
	--disable-libacl \
	--disable-xattr \
	--disable-launch-frontend
