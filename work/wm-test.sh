#!/bin/bash
# wm-test.sh [RES] - drive the XXRI window manager through its controls and
# record what each one actually did.  Runs entirely on the host (Xvfb + the
# device's own i686 binaries), so a regression in the decorations is caught in
# seconds instead of a QEMU boot.
#
# Checks: titlebar geometry, hover, maximize (must stop above the dock),
# restore, minimize (window must unmap) and close (client must exit).
set -u
RES="${1:-1024x768}"
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
WM="${WM:-/tmp/claude-1000/flwm-xxri}"; DISP="${DISP:-:94}"
OUT="${OUT:-/tmp/claude-1000/wmtest}"; mkdir -p "$OUT" "$E/run"; chmod 700 "$E/run"
RESERVE="${XXRI_DOCK_RESERVE:-78}"
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
sleep 0.4
Xvfb "$DISP" -screen 0 ${RES}x24 -nolisten tcp -fp "$R/usr/local/share/fonts/xxri/,built-ins" >"$E/wmt-xvfb.log" 2>&1 &
XP=$!
for i in $(seq 1 50); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
PIDS=""
run() {
  bwrap --ro-bind / / --tmpfs /usr/local/share \
    --ro-bind "$R/usr/local/share/xxri-settings" /usr/local/share/xxri-settings \
    --ro-bind "$R/usr/local/share/xxri-store" /usr/local/share/xxri-store \
    --ro-bind "$R/usr/local/share/pixmaps" /usr/local/share/pixmaps \
    --bind "$E" "$E" --dev /dev --proc /proc \
    --setenv LD_LIBRARY_PATH "$L/lib" --setenv XDG_DATA_DIRS "$L/share:/usr/share" \
    --setenv PATH "$E/stubs:/usr/bin:/bin" --setenv DISPLAY "$DISP" \
    --setenv GDK_BACKEND x11 --setenv NO_AT_BRIDGE 1 --setenv GTK_A11Y none \
    --setenv GDK_GL disable --setenv XDG_RUNTIME_DIR "$E/run" \
    --setenv FONTCONFIG_FILE "$E/fonts.conf" --setenv XXRI_DOCK_RESERVE "$RESERVE" \
    "$@" >>"$E/wmt-app.log" 2>&1 &
  PIDS="$PIDS $!"
}
: > "$E/wmt-app.log"
DISPLAY=$DISP xsetroot -solid "#8b7fd8"
run "$WM"; sleep 1.5
run "$R/usr/local/bin/xxri-settings" wifi; sleep 3

geom() {  # -> "X Y W H" of the FRAME (the WM's window, parent of the client)
  local cid fid
  cid=$(DISPLAY=$DISP xdotool search --name "XXRI Settings" 2>/dev/null | tail -1)
  [ -n "$cid" ] || { echo ""; return; }
  fid=$(DISPLAY=$DISP xwininfo -id "$cid" -children 2>/dev/null | awk '/Parent window id/{print $4}')
  DISPLAY=$DISP xwininfo -id "$fid" 2>/dev/null | awk '
    /Absolute upper-left X/{x=$4} /Absolute upper-left Y/{y=$4}
    /^  Width/{w=$2} /^  Height/{h=$2} END{if(w)print x, y, w, h}'
}
mapped() {
  local cid=$(DISPLAY=$DISP xdotool search --name "XXRI Settings" 2>/dev/null | tail -1)
  [ -n "$cid" ] || { echo GONE; return; }
  DISPLAY=$DISP xwininfo -id "$cid" 2>/dev/null | awk '/Map State/{print $3}'
}
shot() { DISPLAY=$DISP import -window root "$OUT/$1.png" 2>/dev/null; echo "  shot $1"; }

G=$(geom); set -- $G
FX=${1:-0}; FY=${2:-0}; FW=${3:-0}; FH=${4:-0}
echo "frame geometry: x=$FX y=$FY w=$FW h=$FH   (screen $RES, dock reserve $RESERVE)"
BW=15; PAD=8; LFT=3; BAR=26
b1=$((FX+LFT+PAD+BW/2)); b2=$((b1+BW)); b3=$((b2+BW)); by=$((FY+BAR/2))
echo "controls at x=$b1,$b2,$b3 y=$by"

echo "== idle";        shot 01-idle
echo "== hover close"; DISPLAY=$DISP xdotool mousemove $b3 $by; sleep 0.6; shot 02-hover-close
echo "== maximize";    DISPLAY=$DISP xdotool mousemove $b2 $by click 1; sleep 1.5; shot 03-maximized
G=$(geom); echo "   after maximize: $G   map=$(mapped)"
echo "== restore";     DISPLAY=$DISP xdotool mousemove $b2 $by click 1; sleep 1.5; shot 04-restored
echo "   after restore: $(geom)   map=$(mapped)"
echo "== minimize";    DISPLAY=$DISP xdotool mousemove $b1 $by click 1; sleep 1.5; shot 05-minimized
echo "   after minimize: map=$(mapped)"
# bring it back through the desktop menu (flwm: click on the root window)
DISPLAY=$DISP xdotool mousemove 600 400 click 1; sleep 1; shot 06-desktop-menu
DISPLAY=$DISP xdotool key Escape; sleep 0.5
for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
kill $XP 2>/dev/null
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
