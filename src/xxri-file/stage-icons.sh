#!/bin/bash
# stage-icons.sh - render the authoritative XXRI File artwork to PNG and stage
# it into rootfs.  The device has no librsvg / SVG gdk-pixbuf loader, so the
# SVGs in assets/icons/xxri-icons/xxri-files are rasterised here at build time
# rather than at runtime.  These are the exact supplied assets, not substitutes.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BASE=$(cd "$HERE/../.." && pwd)
SRC="$BASE/assets/icons/xxri-icons/xxri-files"
OUT="$BASE/rootfs/usr/local/share/xxri-file/icons"
[ -d "$SRC" ] || { echo "missing asset dir: $SRC" >&2; exit 1; }
mkdir -p "$OUT"
# rail glyphs - fit inside a 26px box, aspect preserved
for n in menu refresh slice30 Photos music Videos Documents Internal-storage Storage-usage; do
    [ -f "$SRC/$n.svg" ] || { echo "MISSING ASSET: $n.svg" >&2; continue; }
    magick -background none -density 1500 "$SRC/$n.svg" -resize 26x26 \
           -gravity center -extent 26x26 "$OUT/$n.png"
    magick -background none -density 1500 "$SRC/$n.svg" -resize 52x52 \
           -gravity center -extent 52x52 "$OUT/$n@2.png"
done
# folder representation: the generic folder, and Applications.  These live one
# level up in the shared XXRI icon set, not in xxri-files/.
XI="$BASE/assets/icons/xxri-icons"
if [ -f "$SRC/folders.svg" ]; then
    magick -background none -density 1500 "$SRC/folders.svg" -resize 26x26 \
           -gravity center -extent 26x26 "$OUT/folder.png"
    magick -background none -density 1500 "$SRC/folders.svg" -resize 52x52 \
           -gravity center -extent 52x52 "$OUT/folder@2.png"
else
    echo "MISSING ASSET: xxri-files/folders.svg" >&2
fi
if [ -f "$XI/all-apps.svg" ]; then
    magick -background none -density 1500 "$XI/all-apps.svg" -resize 26x26 \
           -gravity center -extent 26x26 "$OUT/all-apps.png"
    magick -background none -density 1500 "$XI/all-apps.svg" -resize 52x52 \
           -gravity center -extent 52x52 "$OUT/all-apps@2.png"
else
    echo "MISSING ASSET: all-apps.svg" >&2
fi

# content artwork
magick -background none -density 1500 "$SRC/slice31.svg" -resize x24 "$OUT/title.png"      # "XXRI Files"
magick -background none -density 1500 "$SRC/slice31.svg" -resize x48 "$OUT/title@2.png"
magick -background none -density 1500 "$SRC/slice28.svg" -resize x23 "$OUT/chip-all.png"   # the "All" chip
magick -background none -density 1500 "$SRC/slice28.svg" -resize x46 "$OUT/chip-all@2.png"
echo "staged $(ls "$OUT" | wc -l) files into $OUT"
