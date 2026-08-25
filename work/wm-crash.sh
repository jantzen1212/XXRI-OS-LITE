#!/bin/bash
# wm-crash.sh - reproduce the window-manager crash on the host and, if gdb is
# available, capture a backtrace.  Sequence: open a client, iconify it with the
# XXRI minimise control, then open the desktop menu (which lists the iconified
# window) - the step that killed flwm on the device.
set -u
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
WM="${WM:-/tmp/claude-1000/flwm-xxri}"; DISP="${DISP:-:96}"
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
sleep 0.4
Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp -fp "$R/usr/local/share/fonts/xxri/,built-ins" >/dev/null 2>&1 &
XP=$!
for i in $(seq 1 40); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
mkdir -p "$E/run"; chmod 700 "$E/run"
runbw() {
  bwrap --ro-bind / / --tmpfs /usr/local/share \
    --ro-bind "$R/usr/local/share/xxri-settings" /usr/local/share/xxri-settings \
    --ro-bind "$R/usr/local/share/pixmaps" /usr/local/share/pixmaps \
    --bind "$E" "$E" --dev /dev --proc /proc \
    --setenv LD_LIBRARY_PATH "$L/lib" --setenv XDG_DATA_DIRS "$L/share:/usr/share" \
    --setenv PATH "$E/stubs:/usr/bin:/bin" --setenv DISPLAY "$DISP" \
    --setenv GDK_BACKEND x11 --setenv NO_AT_BRIDGE 1 --setenv GTK_A11Y none \
    --setenv XDG_RUNTIME_DIR "$E/run" --setenv FONTCONFIG_FILE "$E/fonts.conf" \
    --setenv HOME "$E" "$@"
}
if [ "${GDB:-}" = 1 ] && command -v gdb >/dev/null; then
  runbw gdb -q -batch -ex run -ex bt -ex "info registers eip" --args "$WM" \
      >"$E/wm-gdb.log" 2>&1 &
else
  runbw "$WM" >"$E/wm-crash.log" 2>&1 &
fi
WPID=$!
sleep 2
runbw "$R/usr/local/bin/xxri-settings" about >/dev/null 2>&1 &
APID=$!
sleep 6
alive() { pgrep -x "$(basename "$WM")" >/dev/null && echo up || echo DOWN; }
echo "after client map      : wm=$(alive)"
shot() { DISPLAY=$DISP import -window root "/tmp/claude-1000/crash-$1.png" 2>/dev/null; }
shot 0-mapped
DISPLAY=$DISP xdotool mousemove 18 13 click 1; sleep 2
echo "after iconify click   : wm=$(alive)"
shot 1-iconified
DISPLAY=$DISP xdotool mousemove 500 300 mousedown 1; sleep 1.5
echo "after menu press      : wm=$(alive)"
shot 2-menu
DISPLAY=$DISP xdotool mousemove 520 320; sleep 0.6
DISPLAY=$DISP xdotool mouseup 1; sleep 2
echo "after menu release    : wm=$(alive)"
shot 3-after
sleep 1
[ "${GDB:-}" = 1 ] && { echo "--- gdb ---"; tail -30 "$E/wm-gdb.log"; } || { echo "--- stderr ---"; tail -12 "$E/wm-crash.log"; }
kill -9 $APID $WPID 2>/dev/null
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
