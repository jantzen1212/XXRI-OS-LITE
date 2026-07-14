#!/bin/bash
# build.sh - render splash assets to raw RGBA and cross-compile the splash (i686).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../../.." && pwd)/rootfs   # .../rootfs
[ -d "$ROOT/usr/local" ] || ROOT=/home/jantzen/xxri-build/rootfs
ASSET="$ROOT/usr/local/share/xxri-bootsplash"
BIN="$ROOT/usr/local/sbin/xxri-bootsplash"
LOGO="$ROOT/usr/local/share/xxri-settings/icons/logo-big.png"
FONT="$ROOT/usr/local/share/fonts/xxri/AvenirNext-DemiBold.ttf"
[ -f "$FONT" ] || FONT="$ROOT/usr/local/share/fonts/xxri/AvenirNext-Bold.ttf"
mkdir -p "$ASSET" "$(dirname "$BIN")"

rawhdr() {  # png -> width-tall RGBA with 8-byte LE header (w,h)
	local png="$1" out="$2"
	local wh; wh=$(magick "$png" -format "%w %h" info:)
	local w=${wh% *} h=${wh#* }
	magick "$png" -depth 8 RGBA:"$ASSET/.tmp.raw"
	python3 - "$w" "$h" "$ASSET/.tmp.raw" "$out" <<'PY'
import struct,sys
w,h,src,dst=int(sys.argv[1]),int(sys.argv[2]),sys.argv[3],sys.argv[4]
open(dst,'wb').write(struct.pack('<II',w,h)+open(src,'rb').read())
PY
	rm -f "$ASSET/.tmp.raw"
	echo "  $(basename "$out"): ${w}x${h}"
}

echo ">> logo asset"
magick "$LOGO" -resize 176x -background none "$ASSET/.logo.png"
rawhdr "$ASSET/.logo.png" "$ASSET/logo.rgba"; rm -f "$ASSET/.logo.png"

echo ">> title asset"
magick -background none -fill '#EFEDF8' -font "$FONT" -pointsize 27 \
	label:'Starting XXRI OS Lite...' -trim +repage "$ASSET/.title.png"
rawhdr "$ASSET/.title.png" "$ASSET/title.rgba"; rm -f "$ASSET/.title.png"

echo ">> compile (i686)"
CC="gcc -m32 -O2 -Wall"
if $CC -static "$HERE/xxri-bootsplash.c" -o "$BIN" -lm 2>/dev/null; then
	echo "  static build"
else
	$CC "$HERE/xxri-bootsplash.c" -o "$BIN" -lm
	echo "  dynamic build"
fi
strip "$BIN" 2>/dev/null || true
chmod 0755 "$BIN"
file "$BIN"; ls -l "$BIN"
