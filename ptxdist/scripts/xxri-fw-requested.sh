#!/bin/bash
# xxri-fw-requested.sh MODULE TCZ...
# The firmware names a kernel module declares (MODULE_FIRMWARE, i.e. what it
# can request at probe time), read from the pinned Tiny Core module packages.
# config/firmware/*.list start from this output; each list's header says
# which names were then left out and why.
#   scripts/xxri-fw-requested.sh amdgpu src/xxri-cde/graphics-6.12.11-tinycore.tcz
#   scripts/xxri-fw-requested.sh nouveau src/xxri-cde/nouveau-6.12.11-tinycore.tcz
set -euo pipefail
mod=$1; shift
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
for t in "$@"; do
	unsquashfs -q -n -f -d "$W/x" "$t" >/dev/null
done
ko=$(find "$W/x" -name "$mod.ko*" | head -1)
[ -n "$ko" ] || { echo "$mod: no such module in $*" >&2; exit 1; }
case "$ko" in
*.gz) gzip -dc "$ko" > "$W/m.ko" ;;
*.xz) xz -dc "$ko" > "$W/m.ko" ;;
*) cp "$ko" "$W/m.ko" ;;
esac
modinfo -F firmware "$W/m.ko" | sort -u
