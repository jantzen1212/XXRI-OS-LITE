#!/bin/bash
# csd-test.sh - do the application-drawn window controls actually receive
# clicks?  Runs Settings under Xvfb with the real window manager and drives the
# controls with xdotool, which is seconds per iteration instead of minutes.
set -u
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
DISP="${DISP:-:88}"; BIN="${BIN:-/tmp/claude-1000/xxri-settings}"
WM="${WM:-/tmp/claude-1000/flwm-anim}"
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
sleep 0.4
Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp -fp "$R/usr/local/share/fonts/xxri/,built-ins" >/dev/null 2>&1 &
XP=$!
for i in $(seq 1 50); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
mkdir -p "$E/run"; chmod 700 "$E/run"; : > "$E/csd.log"
run() {
  bwrap --ro-bind / / --tmpfs /usr/local/share \
    --ro-bind "$R/usr/local/share/xxri-settings" /usr/local/share/xxri-settings \
    --ro-bind "$R/usr/local/share/pixmaps" /usr/local/share/pixmaps \
    --ro-bind "$R/usr/local/share/fonts" /usr/local/share/fonts \
    --bind "$E" "$E" --dev /dev --proc /proc \
    --setenv LD_LIBRARY_PATH "$L/lib" --setenv XDG_DATA_DIRS "$L/share:/usr/share" \
    --setenv PATH "$E/stubs:/usr/bin:/bin" --setenv DISPLAY "$DISP" \
    --setenv GDK_BACKEND x11 --setenv NO_AT_BRIDGE 1 --setenv GTK_A11Y none \
    --setenv GDK_GL disable --setenv XDG_RUNTIME_DIR "$E/run" \
    --setenv FONTCONFIG_FILE "$E/fonts.conf" --setenv XXRI_DOCK_RESERVE 78 \
    --setenv HOME "$E" "$@" >>"$E/csd.log" 2>&1 &
}
DISPLAY=$DISP xsetroot -solid "#6a5acd"
run "$WM"; sleep 1.5
run "$BIN" about; sleep 5
state() {
  local id=$(DISPLAY=$DISP xdotool search --name "XXRI Settings" 2>/dev/null | tail -1)
  [ -n "$id" ] || { echo "GONE"; return; }
  DISPLAY=$DISP xwininfo -id "$id" 2>/dev/null | awk '/Map State/{m=$3} /Absolute upper-left X/{x=$4} /Absolute upper-left Y/{y=$4} /^  Width/{w=$2} /^  Height/{h=$2} END{print m" "w"x"h"+"x"+"y}'
}
echo "start        : $(state)"
# find the controls by colour, on the window itself
shot() { DISPLAY=$DISP import -window root /tmp/claude-1000/csd-h.png 2>/dev/null; }
shot
ROW=$(magick /tmp/claude-1000/csd-h.png -crop 80x1+0+11 +repage txt: | awk -F'[,:]' 'NR>1{split($0,a,"#"); h=substr(a[2],1,6); r=strtonum("0x" substr(h,1,2)); g=strtonum("0x" substr(h,3,2)); b=strtonum("0x" substr(h,5,2)); if(r>150&&b>150&&g<100) print $1}' | head -1)
echo "min glyph at x=${ROW:-?} y=11"
echo "-- click MIN"
DISPLAY=$DISP xdotool mousemove 7 11 click 1; sleep 2; echo "   $(state)"
echo "-- click CLOSE"
DISPLAY=$DISP xdotool mousemove 37 11 click 1; sleep 2; echo "   $(state)"
echo "-- app stderr"; tail -6 "$E/csd.log"
# (no pkill -f here: the pattern would match this script and kill it)
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
