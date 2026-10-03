#!/bin/sh
# files-dark.sh - XXRI File's artwork for the dark Mode.
#
# Most of Files' rail and list artwork is coloured and reads on both rails.
# Three pieces are near-black ink and would vanish on the dark rail: the menu
# and refresh glyphs, and the "File" half of the title. Their -dark twins
# (which xxri-ui.c picks while Mode is Dark) are the same pixels with that ink
# turned into Settings' dark-mode ink, #F2F2F4; the coloured "XXRI" of the
# title is left alone (only unsaturated, dark pixels change).
#
# Needs ImageMagick (magick) on the build host; the outputs are committed.
#
#   tools/icons/files-dark.sh            # from the repository root
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
D="$ROOT/src/xxri-file/root/usr/local/share/xxri-file/icons"
for n in menu refresh; do
	for s in "" @2; do
		magick "$D/$n$s.png" -fill '#F2F2F4' -colorize 100 \
			-define png:exclude-chunks=date,time "$D/$n-dark$s.png"
	done
done
for s in "" @2; do
	magick "$D/title$s.png" -channel RGB \
		-fx 'saturation < 0.25 && lightness < 0.6 ? channel(0.949,0.949,0.957) : u' \
		-define png:exclude-chunks=date,time "$D/title-dark$s.png"
done
echo "wrote menu, refresh and title -dark twins in $D"
