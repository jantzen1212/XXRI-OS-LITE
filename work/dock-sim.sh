#!/bin/bash
# dock-sim.sh BG.png OUT.png - simulate exactly what wbar will draw.
#
# wbar scales its background image to the bar rectangle and alpha-composites it
# over the root window, so compositing the candidate over the real wallpaper at
# the real dock geometry is a faithful preview - and it costs a second instead
# of a five-minute build-and-boot.
set -u
B=/home/jantzen/xxri-build
BG="${1:?usage: dock-sim.sh BG.png OUT.png}"; OUT="${2:-/tmp/claude-1000/docksim.png}"
WALL="$B/rootfs/usr/local/share/xxri-theme/wallpapers/xxri-os-lite.jpg"
P="$B/rootfs/usr/local/share/pixmaps"
# measured on the device with xxri-wctl: dock 315x58 at +354+695 on 1024x768
DX=354; DY=695; DW=315; DH=58; ISIZE=40; IDIST=7
PAD=$(( (DW - (6*ISIZE + 5*IDIST)) / 2 ))
# a generous slice of wallpaper around the dock, so edges and shadow are visible
CX=$((DX-60)); CY=$((DY-30)); CW=$((DW+120)); CH=$((DH+70))
magick "$WALL" -crop ${CW}x${CH}+${CX}+${CY} +repage /tmp/claude-1000/_sim_bg.png
magick "$BG" -resize ${DW}x${DH}! /tmp/claude-1000/_sim_pill.png
magick /tmp/claude-1000/_sim_bg.png /tmp/claude-1000/_sim_pill.png \
       -geometry +$((DX-CX))+$((DY-CY)) -composite /tmp/claude-1000/_sim_a.png
CMD=(magick /tmp/claude-1000/_sim_a.png)
i=0
for ic in xxri-store xxri-settings mnttool aterm editor exittc; do
  x=$((DX-CX+PAD+i*(ISIZE+IDIST))); y=$((DY-CY+(DH-ISIZE)/2))
  CMD+=(\( "$P/$ic.png" -resize ${ISIZE}x${ISIZE} \) -geometry +$x+$y -composite)
  i=$((i+1))
done
CMD+=("$OUT")
"${CMD[@]}"
magick "$OUT" -resize 250% "${OUT%.png}-zoom.png"
echo "$OUT  ($(identify -format '%wx%h' "$OUT"))"
