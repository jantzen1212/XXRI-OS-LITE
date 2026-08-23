#!/bin/bash
# ui-pointer.sh - drive the guest's PS/2 pointer through the QEMU monitor and
# screendump the result.  Relative motion, so every move starts by slamming the
# pointer into the top-left corner and stepping out from there.
#   ui-pointer.sh X Y [click] OUT.png
set -u
D=/home/jantzen/xxri-build/work/uiqa
MON="$D/mon.sock"
X="$1"; Y="$2"; CLICK="${3:-no}"; OUT="${4:-$D/shots/pointer.png}"
m() { echo "$1" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1; sleep 0.4; }
m "mouse_move -6000 -6000"          # clamp to 0,0
m "mouse_move $X $Y"
if [ "$CLICK" = click ]; then m "mouse_button 1"; m "mouse_button 0"; sleep 2; fi
sleep 1.5
rm -f "$D/p.ppm"; m "screendump $D/p.ppm"; sleep 2
magick "$D/p.ppm" "$OUT" 2>/dev/null && echo "POINTER $OUT ($X,$Y click=$CLICK)"
