#!/bin/bash
# make-dock-bg.sh [OUT] - generate the XXRI dock background.
#
# wbar alpha-composites this image over the root window, so the pill is made of
# translucency rather than paint: a vertical alpha ramp (brighter at the top,
# where a glass edge would catch the light) over a white->lavender colour ramp,
# with a bright rim drawn just inside the edge.  The result reads as a pane of
# glass laid on the wallpaper instead of a flat plastic bar.
#
# Canvas stays 640x88 - wbar scales it to the bar rectangle, and changing the
# aspect here would distort the rounded ends.
set -u
OUT="${1:-/home/jantzen/xxri-build/rootfs/usr/local/share/xxri-theme/wbar-bg.png}"
W=640; H=88; INSET=3
R=$(( (H - 2*INSET) / 2 ))
X2=$((W-INSET-1)); Y2=$((H-INSET-1))
T=/tmp/claude-1000
# tunables: top/bottom alpha of the veil, and the colours it fades between
A_TOP="${A_TOP:-186}"; A_BOT="${A_BOT:-152}"
C_TOP="${C_TOP:-#FFFFFF}"; C_BOT="${C_BOT:-#D9DEFB}"
RIM_A="${RIM_A:-0.72}"

# 1. colour ramp
magick -size ${W}x${H} gradient:"$C_TOP"-"$C_BOT" "$T/_g_col.png"
# 2. alpha ramp (brighter at the top edge)
magick -size ${W}x${H} gradient:"gray($A_TOP)"-"gray($A_BOT)" "$T/_g_alp.png"
# 3. pill mask
magick -size ${W}x${H} xc:black -fill white -stroke none \
       -draw "roundrectangle $INSET,$INSET $X2,$Y2 $R,$R" "$T/_g_mask.png"
# 4. alpha = ramp * mask, so the pill fades but its edge stays crisp
magick "$T/_g_alp.png" "$T/_g_mask.png" -compose Multiply -composite "$T/_g_a.png"
magick "$T/_g_col.png" "$T/_g_a.png" -alpha off -compose CopyOpacity -composite "$T/_g_pill.png"
# 5. rim: a bright hairline just inside the edge, the way a glass edge catches light
magick "$T/_g_pill.png" \
       -stroke "rgba(255,255,255,$RIM_A)" -strokewidth 1.2 -fill none \
       -draw "roundrectangle $((INSET+1)),$((INSET+1)) $((X2-1)),$((Y2-1)) $R,$R" \
       "$T/_g_rim.png"
# 6. keep the rim inside the pill silhouette (the stroke bleeds a little)
magick "$T/_g_rim.png" "$T/_g_mask.png" -alpha off -compose CopyOpacity -composite "$T/_g_rim2.png"
magick "$T/_g_rim.png" -alpha extract "$T/_g_ra.png"
magick "$T/_g_ra.png" "$T/_g_mask.png" -compose Multiply -composite "$T/_g_ra2.png"
magick "$T/_g_rim.png" "$T/_g_ra2.png" -alpha off -compose CopyOpacity -composite "$OUT"
echo "$OUT  $(identify -format '%wx%h alpha_mean=%[fx:mean.a]' "$OUT")"
