#!/bin/sh
# build-core.sh - repack rootfs/ into core.gz and refresh the ISO.
#
# WHY THIS EXISTS: the previous build left output/core.gz (the repacked
# system) OUT of the ISO -- work/iso/boot/core.gz and XXRI-Lite.iso still
# held the original 2025-04-05 core.gz, so none of the architecture changes
# ever booted.  This script repacks AND copies the result into the ISO tree
# AND rebuilds the ISO, so what you boot is what you built.
#
# Run as root for a setuid-correct image (includes sudo/visudo and keeps
# busybox.suid setuid).  Without root it uses fakeroot and WARNS about any
# file it cannot read.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOTFS="$HERE/rootfs"
OUT="$HERE/output"
ISOTREE="$HERE/work/iso"
ISO="$OUT/XXRI-Lite.iso"          # written into output/, originals untouched
mkdir -p "$OUT"

[ -d "$ROOTFS" ] || { echo "no rootfs at $ROOTFS" >&2; exit 1; }
[ -f "$ISOTREE/boot/vmlinuz" ] || { echo "no kernel at $ISOTREE/boot/vmlinuz" >&2; exit 1; }

echo ">> Repacking $ROOTFS -> $OUT/core.gz"
if [ "$(id -u)" = 0 ]; then
	( cd "$ROOTFS" && find . | cpio -o -H newc --quiet ) | gzip -9 > "$OUT/core.gz"
else
	command -v fakeroot >/dev/null 2>&1 || { echo "need root or fakeroot" >&2; exit 1; }
	UNREAD=$(find "$ROOTFS" -type f ! -readable 2>/dev/null || true)
	if [ -n "$UNREAD" ]; then
		echo "!! WARNING: cannot read (will be MISSING from core.gz); run as root to include:" >&2
		echo "$UNREAD" | sed 's/^/     /' >&2
	fi
	# cpio straight from rootfs preserves the setuid mode bits; fakeroot
	# only fakes ownership to 0:0 for the archive.
	fakeroot sh -c "cd '$ROOTFS' && chown -R 0:0 . 2>/dev/null; find . | cpio -o -H newc --quiet | gzip -9" > "$OUT/core.gz"
fi
ls -l "$OUT/core.gz"

echo ">> Refreshing ISO tree core.gz  (the step that was missing before)"
# The extracted ISO tree is read-only; make the build intermediates writable.
chmod -R u+w "$ISOTREE/boot" 2>/dev/null || true
cp -f "$OUT/core.gz" "$ISOTREE/boot/core.gz"

echo ">> Rebuilding $ISO"
xorriso -as mkisofs -l -J -R -V XXRI_LITE \
	-b boot/isolinux/isolinux.bin -c boot/isolinux/boot.cat \
	-no-emul-boot -boot-load-size 4 -boot-info-table \
	-o "$ISO" "$ISOTREE" 2>/dev/null
command -v isohybrid >/dev/null 2>&1 && isohybrid "$ISO" 2>/dev/null || true

echo ">> Done."
echo "   Live/installer ISO : $ISO"
echo "   Installed-system    : boot output/core.gz + a disk labelled XXRIROOT"
echo "                         (see build-writable-image.sh or install2disk)"
