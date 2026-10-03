#!/bin/sh
# dock-bg-dark.sh - make the dark dock background from the light one.
#
# wbar draws the dock pill from a PNG (dot.wbar's i: line). Dark mode
# (Settings > Wallpaper & Style > Mode) uses wbar-bg-dark.png instead, which
# xxri-dock-pin picks. Same shape and the same top rim, so the two only differ
# in colour: the light pill's white-to-grey ramp becomes Settings' dark
# surfaces, #34353E at the rim to #242124 at the bottom, and the glass is a
# tenth more opaque so it still holds together over a bright wallpaper.
#
# Needs ImageMagick (magick) on the build host; the output is committed.
#
#   tools/icons/dock-bg-dark.sh            # from the repository root
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
D="$ROOT/ptxdist/projectroot/usr/local/share/xxri-theme"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
# where a pixel sits on the light ramp: 0 at the white rim, 1 at the bottom
magick "$D/wbar-bg.png" -alpha off -depth 16 \
	-fx 'ramp=min(1,max(0,(0.998-u.r)/0.138)); channel(0.204,0.208,0.243)*(1-ramp) + channel(0.141,0.129,0.141)*ramp' \
	"$T/rgb.png"
magick "$D/wbar-bg.png" -alpha extract -fx 'u>0 ? min(1,u+0.10) : 0' "$T/alpha.png"
magick "$T/rgb.png" "$T/alpha.png" -alpha off -compose CopyOpacity -composite \
	-define png:exclude-chunks=date,time "$D/wbar-bg-dark.png"
echo "wrote $D/wbar-bg-dark.png"
