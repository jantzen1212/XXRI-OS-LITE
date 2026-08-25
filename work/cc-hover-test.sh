#!/bin/bash
# cc-hover-test.sh - does pointing at the bottom-right corner open the Control
# Center, with no click?  Three shots: idle, pointer in the extreme corner
# (the input-only hotspot), and pointer on the chip itself.
set -u
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
CC="${CC:-/tmp/claude-1000/xxri-cc}"; DISP="${DISP:-:92}"; O=/tmp/claude-1000
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
sleep 0.4
Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp -fp "$R/usr/local/share/fonts/xxri/,built-ins" >/dev/null 2>&1 &
XP=$!
for i in $(seq 1 50); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
mkdir -p "$E/run"; chmod 700 "$E/run"
# the real wallpaper, set the way the device sets it (hsetroot publishes
# _XROOTPMAP_ID, which is what the panel samples for its glass and shadow)
bwrap --ro-bind / / --tmpfs /usr/local/share --bind "$E" "$E" --dev /dev --proc /proc \
  --ro-bind "$R/usr/local/share/xxri-theme" /usr/local/share/xxri-theme \
  --setenv LD_LIBRARY_PATH "$L/lib" --setenv DISPLAY "$DISP" \
  "$SYS/usr/local/bin/hsetroot" -fill /usr/local/share/xxri-theme/wallpapers/xxri-os-lite.jpg \
  >/dev/null 2>&1
bwrap --ro-bind / / --tmpfs /usr/local/share \
  --ro-bind "$R/usr/local/share/xxri-control-center" /usr/local/share/xxri-control-center \
  --ro-bind "$R/usr/local/share/pixmaps" /usr/local/share/pixmaps \
  --ro-bind "$R/usr/local/share/fonts" /usr/local/share/fonts \
  --bind "$E" "$E" --dev /dev --proc /proc \
  --setenv LD_LIBRARY_PATH "$L/lib" --setenv XDG_DATA_DIRS "$L/share:/usr/share" \
  --setenv PATH "$E/stubs:/usr/bin:/bin" --setenv DISPLAY "$DISP" \
  --setenv GDK_BACKEND x11 --setenv NO_AT_BRIDGE 1 --setenv GTK_A11Y none \
  --setenv GDK_GL disable --setenv XDG_RUNTIME_DIR "$E/run" \
  --setenv FONTCONFIG_FILE "$E/fonts.conf" --setenv HOME "$E" \
  --setenv XXRI_CC_DEBUG 1 "$CC" >"$E/cc-hover.log" 2>&1 &
CPID=$!
sleep 4
DISPLAY=$DISP xdotool mousemove 400 300; sleep 1
DISPLAY=$DISP import -window root "$O/cc-1-idle.png" 2>/dev/null
echo "1 idle        -> $(DISPLAY=$DISP xdotool getmouselocation 2>/dev/null)"
DISPLAY=$DISP xdotool mousemove 1020 764; sleep 2
DISPLAY=$DISP import -window root "$O/cc-2-corner.png" 2>/dev/null
echo "2 extreme corner (1020,764) - hotspot"
DISPLAY=$DISP xdotool mousemove 400 300; sleep 3
DISPLAY=$DISP xdotool mousemove 950 720; sleep 2
DISPLAY=$DISP import -window root "$O/cc-3-chip.png" 2>/dev/null
echo "3 on the chip (950,720)"
echo "--- trace ---"; grep -E "hotspot|expand|collapse" "$E/xxri-cc.log" 2>/dev/null | tail -8
kill -9 $CPID 2>/dev/null; kill -9 $XP 2>/dev/null
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
