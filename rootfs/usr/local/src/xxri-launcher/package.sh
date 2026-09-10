#!/bin/bash
# package.sh - build xxri-launcher and stage it into the image tree.
#
# Like the Media player, the drawer is a small GTK3 binary that links only
# against libraries the image already carries, so it needs no extension of its
# own: it goes straight into the base rootfs.  Its icon, stylesheet, .desktop
# entry and dock line are already part of rootfs/ and are picked up by the
# normal image build.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../../.." && pwd)
SYS="$ROOT/work/xxri-file/buildroot"

"$HERE/build.sh" "$SYS" "$HERE/xxri-launcher"
install -m755 "$HERE/xxri-launcher" "$ROOT/rootfs/usr/local/bin/xxri-launcher"

# The dock icon is rendered here rather than shipped pre-baked, so the PNG can
# never drift from the SVG.  -density MUST precede the input: without it
# ImageMagick cannot size the SVG through rsvg-convert and silently falls back
# to its MSVG renderer, which paints an opaque white square behind the tile.
# -strip keeps repeated builds byte-identical (IM stamps a date into every PNG).
if command -v magick >/dev/null 2>&1; then
    magick -background none -density 1500 \
           "$ROOT/assets/icons/xxri-icons/xxri-launcher.svg" \
           -resize 64x64 -gravity center -extent 64x64 -strip \
           "$ROOT/rootfs/usr/local/share/pixmaps/xxri-launcher.png"
fi

# Every runtime dependency is checked here rather than discovered on a booted
# image, the same way the player does it.
missing=0
for n in $(readelf -d "$HERE/xxri-launcher" | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p'); do
    case "$n" in ld-linux*|linux-gate*) continue;; esac
    find "$ROOT/rootfs" "$SYS/usr/local/lib" -name "$n" 2>/dev/null | grep -q . && continue
    found=0
    for t in "$ROOT"/work/iso/cde/optional/*.tcz; do
        unsquashfs -l "$t" 2>/dev/null | grep -q "/$n\$" && { found=1; break; }
    done
    [ $found = 1 ] || { echo "   !! $n is in neither the rootfs nor any extension"; missing=$((missing+1)); }
done
[ "$missing" = 0 ] || { echo "   $missing unresolved libraries" >&2; exit 1; }

echo "xxri-launcher -> $ROOT/rootfs/usr/local/bin/xxri-launcher ($(du -h "$HERE/xxri-launcher" | cut -f1))"
echo "modes: (no args) drawer, --button dock key, --dockmenu unpin popup,"
echo "       --frame-icon SRC DST SIZE  (icon into the XXRI squircle container)"
echo "       --dock-bg SRC DST K N      (dock background with the running-app rule)"
echo "running apps: entries in the dock itself, written by /usr/local/bin/xxri-dock-pin"
echo "pinned apps: /usr/local/share/xxri-launcher/pinned.list, via /usr/local/bin/xxri-dock-pin"
