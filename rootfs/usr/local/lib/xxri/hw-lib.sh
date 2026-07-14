# hw-lib.sh - shared library for the xxri Hardware Enablement Layer.
#
# Sourced by every xxri-* hardware backend.  Provides JSON emission,
# sysfs helpers and the persistent configuration store, so all backends
# speak the same dialect and future frontends (Control Center, Software
# Center, AI) consume one stable API.
#
# Design rules (forward compatibility):
#  - prefer kernel-stable interfaces (/sys, /proc) over tool output
#  - never hardcode Tiny Core paths beyond the config store location
#  - every command supports --json; schemas carry  "api":1  and only
#    ever gain fields, never change or drop them.

XXRI_CFG_DIR=/usr/local/etc/xxri
XXRI_RUN_DIR=/var/run/xxri          # shared, written by boot-time (root) tasks
XXRI_TMP="/tmp/xxri-$(id -u)"       # always writable by the caller
XXRI_API=1

mkdir -p "$XXRI_RUN_DIR" 2>/dev/null
mkdir -p "$XXRI_TMP" 2>/dev/null

# ------------------------------------------------------------- generic --
sf() { cat "$1" 2>/dev/null; }                    # read a sysfs file
have() { command -v "$1" >/dev/null 2>&1; }

as_root() { # re-exec the whole command through sudo when not root
	[ "$(id -u)" = 0 ] && return 0
	exec sudo "$0" "$@"
}
sudo_if() { # run one command as root
	if [ "$(id -u)" = 0 ]; then "$@"; else sudo "$@"; fi
}

# ---------------------------------------------------------------- JSON --
j_esc() { # escape a string for JSON
	printf '%s' "$1" | awk 'BEGIN{RS="\x01"} {
		gsub(/\\/,"\\\\"); gsub(/"/,"\\\"");
		gsub(/\t/,"\\t"); gsub(/\r/,"\\r"); gsub(/\n/,"\\n");
		printf "%s", $0 }'
}
# fragment builders: accumulate into $J (reset with j_new)
j_new()  { J=""; }
j_str()  { J="$J${J:+,}\"$1\":\"$(j_esc "$2")\""; }
j_num()  { case "$2" in ''|*[!0-9.-]*) J="$J${J:+,}\"$1\":null";; *) J="$J${J:+,}\"$1\":$2";; esac; }
j_bool() { [ "$2" = 1 ] || [ "$2" = true ] && J="$J${J:+,}\"$1\":true" || J="$J${J:+,}\"$1\":false"; }
j_raw()  { J="$J${J:+,}\"$1\":$2"; }
j_obj()  { printf '{%s}' "$J"; }

# array accumulation: A="" ; a_add "$(j_obj)" ; a_out
a_new() { A=""; }
a_add() { A="$A${A:+,}$1"; }
a_out() { printf '[%s]' "$A"; }

# root envelope:  j_root backend '"key":...,...'
j_root() { printf '{"api":%s,"backend":"%s",%s}\n' "$XXRI_API" "$1" "$2"; }

# ------------------------------------------------------------- config --
# cfg_set NAME KEY VALUE / cfg_get NAME KEY [default]
cfg_file() { echo "$XXRI_CFG_DIR/$1.conf"; }
cfg_get() {
	f="$(cfg_file "$1")"
	v="$(sed -n "s/^$2=//p" "$f" 2>/dev/null | head -1)"
	[ -n "$v" ] && echo "$v" || echo "$3"
}
cfg_set() {
	f="$(cfg_file "$1")"
	sudo_if mkdir -p "$XXRI_CFG_DIR"
	if [ -f "$f" ] && grep -q "^$2=" "$f" 2>/dev/null; then
		sudo_if sh -c "sed -i 's|^$2=.*|$2=$3|' '$f'"
	else
		sudo_if sh -c "echo '$2=$3' >> '$f'"
	fi
}

# --------------------------------------------------------------- sysfs --
# load the right driver for a sysfs device via its modalias
hw_load_driver() {
	m="$(sf "$1/modalias")"
	[ -n "$m" ] && sudo_if modprobe "$m" 2>/dev/null
}
# PCI: iterate devices of a class prefix: pci_by_class 0x03 -> gpu
pci_by_class() { # $1 = class prefix like 0x03 ; echoes sysfs paths
	for d in /sys/bus/pci/devices/*; do
		case "$(sf "$d/class")" in "$1"*) echo "$d";; esac
	done
}
pci_name() { # $1 sysfs pci device path -> human name (pciutils+hwdata)
	addr="${1##*/}"
	if have lspci; then
		n="$(lspci -s "${addr#0000:}" 2>/dev/null | sed 's/^[^ ]* //')"
		[ -n "$n" ] && { echo "$n"; return; }
	fi
	echo "PCI device $(sf "$1/vendor"):$(sf "$1/device")"
}
usb_devices() { # echoes sysfs paths of USB devices (not interfaces/hubs)
	for d in /sys/bus/usb/devices/[0-9]*-[0-9]*; do
		[ -e "$d/idVendor" ] || continue
		case "$d" in *:*) continue;; esac
		echo "$d"
	done
}

# ----------------------------------------------------------- misc info --
hw_kernel()  { uname -r; }
hw_arch()    { uname -m; }
hw_os_name() { sed -n 's/^PRETTY_NAME="\(.*\)"/\1/p' /etc/os-release 2>/dev/null; }
