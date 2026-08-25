#!/bin/bash
# wctl-test.sh - prove the dock's window control on the host: a real client, a
# real minimise through the titlebar control, and a real restore through
# xxri-wctl activate - which is exactly what a dock click will call.
set -u
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
WM="${WM:-/tmp/claude-1000/flwm-xxri}"; W="${W:-/tmp/claude-1000/xxri-wctl}"
DISP="${DISP:-:95}"; H="$E/wctlhome"
rm -rf "$H"; mkdir -p "$H/.wmx/Applications" "$E/run"; chmod 700 "$E/run"
printf '#!/bin/sh\nexec /usr/local/bin/xxri-settings\n' > "$H/.wmx/Applications/XXRISettings"
chmod 755 "$H/.wmx/Applications/XXRISettings"
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
sleep 0.4
Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp -fp "$R/usr/local/share/fonts/xxri/,built-ins" >/dev/null 2>&1 &
XP=$!
for i in $(seq 1 50); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
PIDS=""
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
    --setenv HOME "$H" "$@" >>"$E/wctl-app.log" 2>&1 &
  PIDS="$PIDS $!"
}
: > "$E/wctl-app.log"
DISPLAY=$DISP xsetroot -solid "#8b7fd8" 2>/dev/null
run "$WM"; sleep 1.5
run "$R/usr/local/bin/xxri-settings" about; sleep 4
wctl() { DISPLAY=$DISP "$W" "$@"; }
echo "1. list after launch:";        wctl list | sed 's/^/     /'
echo "2. running xxri-settings? ->"; wctl running xxri-settings && echo "     YES (exit 0)" || echo "     NO"
echo "3. running gimp? ->";          wctl running gimp && echo "     YES" || echo "     NO (exit 1, correct)"
echo "4. minimise with the titlebar control"
DISPLAY=$DISP xdotool mousemove 18 13 click 1; sleep 1.5
wctl list | sed 's/^/     /'
echo "5. activate it again (what a dock click will do)"
wctl activate xxri-settings && echo "     activate returned 0" || echo "     activate FAILED"
sleep 1.5
wctl list | sed 's/^/     /'
DISPLAY=$DISP import -window root /tmp/claude-1000/wctl-restored.png 2>/dev/null
for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
kill -9 $XP 2>/dev/null
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
