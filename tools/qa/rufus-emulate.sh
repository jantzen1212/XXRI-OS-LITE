#!/bin/bash
# tools/qa/rufus-emulate.sh ISO OUT.img mbr|gpt [RUFUS_CORE_IMG]   (QA only)
#
# Rufus is Windows-only and needs raw disk access, so its "ISO Image mode" is
# emulated from what Rufus documents and does for a GRUB 2 ISO:
#   * one FAT32 partition from 1 MiB, labelled with the ISO volume id;
#     MBR: type 0x0C + active flag; GPT: Microsoft basic data (Rufus offers
#     GPT only for UEFI targets)
#   * every file of the ISO copied onto it (the ISO9660 volume, El Torito and
#     hybrid MBR are NOT carried over - which is why the loaders must find
#     their files by marker file, not by ISO UUID)
#   * MBR only: GRUB boot.img as MBR code and Rufus's OWN core.img in the gap
#     at sector 1 - Rufus downloads a version-matched one from
#     https://rufus.ie/files/grub-<version>/core.img (built with prefix
#     (hd0,msdos1)/boot/grub and biosdisk part_msdos fat ntfs exfat)
set -euo pipefail
ISO=$1 OUT=$2 SCHEME=$3 CORE=${4:-}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
SIZE_MIB=1024
rm -f "$OUT"; truncate -s ${SIZE_MIB}M "$OUT"
if [ "$SCHEME" = mbr ]; then
	printf 'label: dos\n2048,,c,*\n' | sfdisk -q "$OUT"
else
	sgdisk -n 1:2048:0 -t 1:0700 -c 1:XXRI_LITE "$OUT" >/dev/null
fi
START=2048
END=$(sfdisk -d "$OUT" | awk -F'[=,]' '/start=/{s=$2; z=$4; gsub(/ /,"",s); gsub(/ /,"",z); print s+z; exit}')
KIB=$(( (END - START) / 2 ))
mkfs.fat -F 32 --offset $START -n XXRI_LITE "$OUT" $KIB >/dev/null
xorriso -osirrox on -indev "$ISO" -extract / "$W/tree" >/dev/null 2>&1
chmod -R u+w "$W/tree"
MTOOLS_SKIP_CHECK=1 mcopy -s -i "$OUT@@$((START * 512))" "$W"/tree/* "$W"/tree/.disk ::/
if [ "$SCHEME" = mbr ] && [ -n "$CORE" ]; then
	# GRUB's boot.img as shipped on the ISO itself (the same upstream bits
	# Rufus writes), so this works from any checkout
	dd if="$W/tree/boot/grub/i386-pc/boot.img" of="$OUT" bs=440 count=1 conv=notrunc 2>/dev/null
	dd if="$CORE" of="$OUT" bs=512 seek=1 conv=notrunc 2>/dev/null
fi
[ "$SCHEME" = mbr ] && [ -n "$CORE" ] && { [ "$(head -c 2 "$OUT" | xxd -p)" = eb63 ] ||
	{ echo "rufus-emulate: no GRUB boot code in the MBR" >&2; exit 1; }; }
echo "rufus-emulated $SCHEME image: $OUT"
