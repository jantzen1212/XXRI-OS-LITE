# Wi-Fi and Bluetooth in XXRI OS Lite 2.6

What ships, what the backends and Settings do with it, and what has and has
not been tested. Branch `xxri-gpu-cpu-support`.

## What ships

| Piece | Where from | Size |
|---|---|---|
| `xxri-firmware-wireless.tcz` - 154 firmware files + 24 links from linux-firmware 20260810 (`config/firmware/wireless.list`, resolved through WHENCE by `scripts/xxri-fw-select.py`), plus `regulatory.db`/`.p7s` from wireless-regdb 2025.10.07 | `rules/xxri-firmware-wireless.make` (regdb is a second `_PARTS` source) | 13.2 MiB |
| `firmware-ipw2100`, `firmware-ipw2200` (Intel's own licence), `firmware-zd1211`, `firmware-openfwwf` (open b43 firmware) - not in linux-firmware | Tiny Core 16.x, md5-pinned in `config/cde/manifest` | 0.7 MiB |
| `iw`, `rfkill` | Tiny Core | 0.1 MiB |
| `bluetooth-6.12.11-tinycore` (btusb, ath3k, btintel, btrtl, btbcm, hci_uart ...), `bluez5` 5.72 | Tiny Core | 2.0 MiB |

All of it is in `onboot.lst`, after `wifi.tcz`. `dep/bluez5.tcz.dep` is
XXRI's: Tiny Core's also pulls `libical3` (with icu70, 15 MB) and `sbc`,
which only `obexd` (OBEX file transfer) links; `bluetoothd` and
`bluetoothctl` need glib2, dbus, readline and alsa-lib, all already loaded.

The Wi-Fi and Bluetooth drivers themselves are Tiny Core's kernel packages
(`wireless-6.12.11-tinycore`, `bluetooth-6.12.11-tinycore`). The kernel has
`CFG80211_REQUIRE_SIGNED_REGDB` off and `FW_LOADER_USER_HELPER_FALLBACK` on,
so `regulatory.db` and the firmware under `/usr/local/lib/firmware` are
served by Tiny Core's udev helper (checked against
`config-6.12.11-tinycore`).

### Scope

Machines and adapters up to about 2018: 802.11 a/b/g/n and 802.11ac (Wi-Fi
5) on PCI, PCIe, CardBus and USB, and the Bluetooth half of the same combo
cards and dongles. `wireless.list`'s header lists what is left out and what
those devices do without it; in short:

* Wi-Fi 6/6E/7 (Intel AX/BE, Realtek 8852/8922, MediaTek 7921/7922/7925,
  Qualcomm ath11k/ath12k): the driver loads and the radio does not start.
* Intel's Wi-Fi 5 CNVi parts on Ice Lake and later platforms.
* SDIO and board-specific parts (ath6kl, brcmfmac SDIO, wl12xx/wl18xx,
  rtw88 SDIO ...): they need per-board calibration files.
* Broadcom b43 LP/N-PHY cores (BCM4312, BCM4321/4322, BCM43227/43228) and
  b43legacy: Broadcom forbids redistributing that firmware. Not supported.

Supported only with a kernel rebuild (no driver in Tiny Core's kernel
packages): RTL8723BS (SDIO), RT35xx USB, 16-bit PCMCIA Wi-Fi.

## Backends

`xxri-network` (rewritten) and `xxri-bluetooth` (new); the full command list
is in `/usr/local/share/doc/xxri/hardware-api.md`.

**Wi-Fi.** On/off is the rfkill soft switch of every `wlan` radio (laptop
switches from acer-wmi, thinkpad_acpi and the like included), kept across
boots. All saved networks go to wpa_supplicant as network blocks at once and
wpa_supplicant joins the one in range. The old loop tried them one by one,
which cost 20 s per absent network and let a later profile take the radio
from one that had connected. A connect attempt tells a refused key (the
4-way handshake started and failed twice) from a network that never answered.
SSIDs come back from `wpa_cli` printf-escaped (`\xNN`, `\"`) and are decoded
before they are compared, shown or saved, so non-ASCII and quoted names
work. A static address saved for an interface is applied after association
instead of DHCP, and switching an interface back to DHCP removes the static
profile.

On an installed system `/var/run` is on disk, so state left by a crash or
power cut survives the reboot. The backends therefore ask processes rather
than trusting files: `wpa_cli ping`, `/proc/PID/comm` before killing a
udhcpc named in a pid file, and `kill -0` on D-Bus's pid file.

**Bluetooth.** `start` does what `bluez5.tcz`'s install hook does on a live
boot (copy `main.conf` & co. into `/usr/local/etc/bluetooth`); installed
systems never run that hook. It then brings up the system D-Bus (nothing else
in XXRI needs it, so it starts on demand) and `bluetoothd`, and sets the name
phones see to the computer's host name unless Settings set another. Pairing is
one interactive `bluetoothctl` on a FIFO with a KeyboardDisplay agent. The
backend parses the agent's questions out of its output (the prompts are
readline prompts with no newline, so it greps the whole output) and passes
them to the caller, as described in hardware-api.md. Older devices that ask
for a PIN get 0000 (headsets), or for keyboards a random six-digit PIN to
type on the device. SIGTERM (Settings' Cancel) sends `cancel-pairing` on the
way out.

**Boot.** `xxri-hw-init` runs `xxri-network auto` and `xxri-bluetooth auto`
in the background. `auto` for Bluetooth starts the stack only when an
adapter is present and Bluetooth is not switched off; when it is switched
off, it re-applies the rfkill block.

## Settings

Wi-Fi & Network and Bluetooth are Samsung-style pages in XXRI's styling:
an On/Off pill (blue when on), the current connection with a gear button
(details sheet with Forget for Wi-Fi; Connect/Disconnect/Unpair menu for a
device), then what is around. Wi-Fi networks show drawn signal bars, a lock
and a 5G badge, and rescan every 20 s while the page is open. Tapping a
saved or open network connects, a secured one asks for the password and asks
again when the key is refused. "Add network" covers hidden networks;
enterprise (802.1X) networks are refused with an explanation. The Bluetooth
page scans once when it opens. Pairing shows the code to compare, with Pair
and Cancel, or the passkey/PIN to type. Every backend call is asynchronous
(GSubprocess), so a scan no longer freezes the window. Control Center's
Bluetooth tile now says On, Off or Unavailable.

## Testing

* Both backends run on the device's own busybox (bwrap over the 2.6 root)
  against stateful fakes of `wpa_cli` and `bluetoothctl`. The fakes include
  `bluetoothctl`'s interactive mode on a FIFO with the agent's prompts.
  Covered: scan deduplication and hidden-SSID filtering, UTF-8 and quoted
  SSIDs, a wrong key against an absent network, multi-profile reconnect,
  passwordless reconnect to a saved network, hidden networks, forget, radio
  off/on, static versus DHCP precedence, Bluetooth status/power/scan/devices,
  numeric confirmation, keyboard PIN, passkey display, failed and unknown
  devices, Cancel, connect/disconnect/remove and rename.
* The agent's strings, result messages and readline-on-FIFO behaviour were
  checked against the BlueZ 5.72 source.
* Settings pages: rendered in every state (on, off, no adapter, hard block,
  dialogs, dark mode) with the host render harness.

**Not tested: real radios.** QEMU has no Wi-Fi or Bluetooth device (QEMU's
`usb-bt-dongle` was removed in 5.0, and Tiny Core's kernel has no
`mac80211_hwsim`). The parts that only real hardware can show are
association with a real access point, DHCP on it, BlueZ talking to a real
controller (firmware download included) and real pairing.
