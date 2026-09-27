#!/bin/bash
# tools/qa/ventoy-emulate.sh VENTOY_DIR ISO OUT.img [mbr|gpt]   (QA only)
#
# Replays Ventoy2Disk.sh's install (tool/VentoyWorker.sh + format_ventoy_disk_
# mbr/_gpt in tool/ventoy_lib.sh; gpt = Ventoy2Disk.sh -g) onto an image file, because
# the real script needs root and a block device.  Every boot component is
# Ventoy's own (boot/boot.img, boot/core.img.xz, ventoy/ventoy.disk.img.xz,
# tool/x86_64/vtoycli); the one substitution is the data partition, FAT32
# instead of exFAT (Ventoy supports both), so the ISO can be copied in with
# mtools.  ventoy/ventoy.json (Ventoy's documented plugin file) makes Ventoy
# boot the first image after 3 s, so no key presses are needed.
set -euo pipefail
V=$(readlink -f "$1") ISO=$(readlink -f "$2") OUT=$3 STYLE=${4:-mbr}
SIZE_MIB=2048 VSEC=65536
rm -f "$OUT"; truncate -s ${SIZE_MIB}M "$OUT"
N=$(( SIZE_MIB * 2048 ))
VTOYCLI="${OUT}.vtoycli"; xz -dc "$V/tool/x86_64/vtoycli.xz" > "$VTOYCLI"; chmod +x "$VTOYCLI"
P1S=2048
if [ "$STYLE" = gpt ]; then P1E=$(( N - VSEC - 34 )); else P1E=$(( N - VSEC - 1 )); fi
P2S=$(( P1E + 1 ))
M=$(( P2S % 8 )); [ $M -gt 0 ] && { P1E=$(( P1E - M )); P2S=$(( P1E + 1 )); }
if [ "$STYLE" = gpt ]; then
	parted -a none --script "$OUT" mklabel gpt unit s mkpart Ventoy ntfs $P1S $P1E \
		mkpart VTOYEFI fat16 $P2S $(( P2S + VSEC - 1 )) set 2 msftdata on
	"$VTOYCLI" gpt -f "$OUT" >/dev/null 2>&1
else
	printf 'label: dos\n%d,%d,c,*\n%d,%d,ef\n' $P1S $(( P1E - P1S + 1 )) $P2S $VSEC | sfdisk -q "$OUT"
fi
mkfs.fat -F 32 --offset $P1S -n Ventoy "$OUT" $(( (P1E - P1S + 1) / 2 )) >/dev/null
dd if="$V/boot/boot.img" of="$OUT" bs=1 count=446 conv=notrunc status=none
if [ "$STYLE" = gpt ]; then
	printf '\x22' | dd of="$OUT" bs=1 count=1 seek=92 conv=notrunc status=none
	xzcat "$V/boot/core.img.xz" | dd of="$OUT" bs=512 count=2014 seek=34 conv=notrunc status=none
	printf '\x23' | dd of="$OUT" bs=1 count=1 seek=17908 conv=notrunc status=none
else
	xzcat "$V/boot/core.img.xz" | dd of="$OUT" bs=512 count=2047 seek=1 conv=notrunc status=none
fi
xzcat "$V/ventoy/ventoy.disk.img.xz" | dd of="$OUT" bs=512 count=$VSEC seek=$P2S conv=notrunc status=none
head -c 16 /dev/urandom | dd of="$OUT" seek=384 bs=1 count=16 conv=notrunc status=none
head -c 4 /dev/urandom | dd of="$OUT" seek=440 bs=1 count=4 conv=notrunc status=none
# Ventoy2Disk.sh without -s (Secure Boot support off) runs this on the ESP.
"$VTOYCLI" partresize -s "$OUT" $P2S >/dev/null 2>&1 ||
	echo "note: vtoycli partresize -s did not run on an image file (Secure Boot shim stays in place)"
rm -f "$VTOYCLI"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
mkdir -p "$W/ventoy"
printf '{ "control": [ { "VTOY_MENU_TIMEOUT": "3" }, { "VTOY_SECONDARY_BOOT_MENU": "0" } ] }\n' > "$W/ventoy/ventoy.json"
export MTOOLS_SKIP_CHECK=1
mcopy -s -i "$OUT@@$(( P1S * 512 ))" "$W/ventoy" ::/
mcopy -i "$OUT@@$(( P1S * 512 ))" "$ISO" ::/
echo "ventoy $(cat "$V/ventoy/version") $STYLE image: $OUT (ISO $(basename "$ISO") in partition 1)"
