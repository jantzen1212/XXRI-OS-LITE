# -*-makefile-*-
#
# xxri-firmware-wireless - firmware for Wi-Fi and Bluetooth adapters up to the
# 802.11ac generation, and the wireless regulatory database.
# Only the files config/firmware/wireless.list names, resolved through the
# release's WHENCE by scripts/xxri-fw-select.py (an unknown name fails the
# build). Loaded at boot; Tiny Core's udev helper serves /usr/local/lib/firmware
# through the sysfs fallback, regulatory.db included (cfg80211 asks for it
# like any firmware and then accepts the country Settings sets).
#
# ", n": no stripping, firmware must stay bit-identical.
#
PACKAGES-$(PTXCONF_XXRI_FIRMWARE_WIRELESS) += xxri-firmware-wireless

XXRI_FIRMWARE_WIRELESS_VERSION	:= $(XXRI_LINUX_FIRMWARE_VERSION)
XXRI_FIRMWARE_WIRELESS_SHA256	:= $(XXRI_LINUX_FIRMWARE_SHA256)
XXRI_FIRMWARE_WIRELESS		:= xxri-firmware-wireless-$(XXRI_FIRMWARE_WIRELESS_VERSION)
XXRI_FIRMWARE_WIRELESS_URL	:= $(XXRI_LINUX_FIRMWARE_URL)
XXRI_FIRMWARE_WIRELESS_SOURCE	:= $(XXRI_LINUX_FIRMWARE_SOURCE)
XXRI_FIRMWARE_WIRELESS_DIR	:= $(BUILDDIR)/$(XXRI_FIRMWARE_WIRELESS)
XXRI_FIRMWARE_WIRELESS_LICENSE	:= LicenseRef-linux-firmware-wireless AND ISC
XXRI_FIRMWARE_WIRELESS_DEVPKG	:= NO
XXRI_FIRMWARE_WIRELESS_LIST	:= $(PTXDIST_WORKSPACE)/config/firmware/wireless.list
XXRI_FIRMWARE_WIRELESS_SELECT	:= $(PTXDIST_WORKSPACE)/scripts/xxri-fw-select.py

# the release PTXdist 2026.09's crda rule pins (rules/crda.make)
XXRI_FIRMWARE_WIRELESS_REGDB_VERSION	:= 2025.10.07
XXRI_FIRMWARE_WIRELESS_REGDB_SHA256	:= 7c3dff164d35db9fa99eaf8bf3b0293cd7401a7af7dff3f4b9bdb341022281ef
XXRI_FIRMWARE_WIRELESS_REGDB		:= wireless-regdb-$(XXRI_FIRMWARE_WIRELESS_REGDB_VERSION)
XXRI_FIRMWARE_WIRELESS_REGDB_URL	:= \
	https://www.kernel.org/pub/software/network/wireless-regdb/$(XXRI_FIRMWARE_WIRELESS_REGDB).tar.gz
XXRI_FIRMWARE_WIRELESS_REGDB_SOURCE	:= $(SRCDIR)/$(XXRI_FIRMWARE_WIRELESS_REGDB).tar.gz
XXRI_FIRMWARE_WIRELESS_REGDB_DIR	:= $(XXRI_FIRMWARE_WIRELESS_DIR)
XXRI_FIRMWARE_WIRELESS_PARTS		+= XXRI_FIRMWARE_WIRELESS_REGDB

# 650 MB tarball: nothing is unpacked in extract. The selection runs in
# prepare (host Python exists by then) and again when the list or script changes.
$(STATEDIR)/xxri-firmware-wireless.extract:
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_WIRELESS_DIR) && mkdir -p $(XXRI_FIRMWARE_WIRELESS_DIR)
	@$(call touch)

$(STATEDIR)/xxri-firmware-wireless.prepare: $(XXRI_FIRMWARE_WIRELESS_LIST) $(XXRI_FIRMWARE_WIRELESS_SELECT)
	@$(call targetinfo)
	@rm -rf $(XXRI_FIRMWARE_WIRELESS_DIR)/firmware $(XXRI_FIRMWARE_WIRELESS_DIR)/licenses \
		$(XXRI_FIRMWARE_WIRELESS_DIR)/$(XXRI_FIRMWARE_WIRELESS_REGDB)
	@$(XXRI_HOST_PYTHON) $(XXRI_FIRMWARE_WIRELESS_SELECT) extract $(XXRI_FIRMWARE_WIRELESS_SOURCE) \
		$(XXRI_FIRMWARE_WIRELESS_DIR)/firmware $(XXRI_FIRMWARE_WIRELESS_LIST)
	@tar -xzf $(XXRI_FIRMWARE_WIRELESS_REGDB_SOURCE) -C $(XXRI_FIRMWARE_WIRELESS_DIR) \
		$(XXRI_FIRMWARE_WIRELESS_REGDB)/regulatory.db \
		$(XXRI_FIRMWARE_WIRELESS_REGDB)/regulatory.db.p7s \
		$(XXRI_FIRMWARE_WIRELESS_REGDB)/LICENSE
	@$(call touch)

$(STATEDIR)/xxri-firmware-wireless.compile $(STATEDIR)/xxri-firmware-wireless.install:
	@$(call targetinfo)
	@$(call touch)

$(STATEDIR)/xxri-firmware-wireless.targetinstall:
	@$(call targetinfo)
	@$(call install_init, xxri-firmware-wireless)
	@$(call install_fixup, xxri-firmware-wireless,PRIORITY,optional)
	@$(call install_fixup, xxri-firmware-wireless,SECTION,base)
	@$(call install_fixup, xxri-firmware-wireless,AUTHOR,"XXRI OS Team")
	@$(call install_fixup, xxri-firmware-wireless,DESCRIPTION,"firmware for Wi-Fi and Bluetooth adapters up to 802.11ac")
	@$(call install_tree, xxri-firmware-wireless, 0, 0, $(XXRI_FIRMWARE_WIRELESS_DIR)/firmware, /usr/local/lib/firmware, n)
#	# unsigned is fine: this kernel has CFG80211_REQUIRE_SIGNED_REGDB off;
#	# the signature ships for a kernel that wants it
	@$(call install_copy, xxri-firmware-wireless, 0, 0, 0644, \
		$(XXRI_FIRMWARE_WIRELESS_DIR)/$(XXRI_FIRMWARE_WIRELESS_REGDB)/regulatory.db, \
		/usr/local/lib/firmware/regulatory.db, n)
	@$(call install_copy, xxri-firmware-wireless, 0, 0, 0644, \
		$(XXRI_FIRMWARE_WIRELESS_DIR)/$(XXRI_FIRMWARE_WIRELESS_REGDB)/regulatory.db.p7s, \
		/usr/local/lib/firmware/regulatory.db.p7s, n)
#	# the redistribution terms travel with the files
	@$(call install_tree, xxri-firmware-wireless, 0, 0, $(XXRI_FIRMWARE_WIRELESS_DIR)/licenses, \
		/usr/local/share/doc/xxri-firmware-wireless, n)
	@$(call install_copy, xxri-firmware-wireless, 0, 0, 0644, \
		$(XXRI_FIRMWARE_WIRELESS_DIR)/$(XXRI_FIRMWARE_WIRELESS_REGDB)/LICENSE, \
		/usr/local/share/doc/xxri-firmware-wireless/LICENSE.wireless-regdb)
	@$(call install_copy, xxri-firmware-wireless, 0, 0, 0644, $(XXRI_FIRMWARE_WIRELESS_LIST), \
		/usr/local/share/doc/xxri-firmware-wireless/wireless.list)
	@$(call install_finish, xxri-firmware-wireless)
	@$(call touch)

# vim: syntax=make
