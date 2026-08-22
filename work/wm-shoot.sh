#!/bin/bash
# wm-shoot.sh RES OUT.png [client...] - render the XXRI window manager on the
# host, exactly as the device will, and screenshot the whole root window.
#
# Same trick as the Settings harness: the i686 binaries run against the
# device's own libraries (~/.xxri-testenv/sys) under Xvfb + bubblewrap, so a
# clean host render is a faithful preview of the device. QEMU stays the
# authority; this is the 3-second iteration loop.
set -u
RES="${1:-1024x768}"; OUT="${2:-/tmp/claude-1000/wm.png}"; shift 2 || true
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
WM="${WM:-/tmp/claude-1000/flwm-xxri}"
DISP="${DISP:-:93}"
mkdir -p "$E/run"; chmod 700 "$E/run"
pkill -f "Xvfb $DISP" 2>/dev/null; sleep 0.4
# -fp: the XXRI face is published to the X server as the "xxri" foundry, the
# same way the device does it (Xvesa -fp / xset +fp under X.Org).
Xvfb "$DISP" -screen 0 ${RES}x24 -nolisten tcp \
     -fp "$R/usr/local/share/fonts/xxri/,built-ins" >"$E/wm-xvfb.log" 2>&1 &
XP=$!
for i in $(seq 1 50); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
run() {  # run a command inside the device runtime
  bwrap --ro-bind / / --tmpfs /usr/local/share \
    --ro-bind "$R/usr/local/share/xxri-settings" /usr/local/share/xxri-settings \
    --ro-bind "$R/usr/local/share/xxri-store" /usr/local/share/xxri-store \
    --ro-bind "$R/usr/local/share/xxri-control-center" /usr/local/share/xxri-control-center \
    --ro-bind "$R/usr/local/share/pixmaps" /usr/local/share/pixmaps \
    --ro-bind "$R/usr/local/share/fonts" /usr/local/share/fonts \
    --bind "$E" "$E" --dev /dev --proc /proc \
    --setenv LD_LIBRARY_PATH "$L/lib" --setenv XDG_DATA_DIRS "$L/share:/usr/share" \
    --setenv PATH "$E/stubs:/usr/bin:/bin" --setenv DISPLAY "$DISP" \
    --setenv GDK_BACKEND x11 --setenv NO_AT_BRIDGE 1 --setenv GTK_A11Y none \
    --setenv GDK_GL disable --setenv XDG_RUNTIME_DIR "$E/run" \
    --setenv FONTCONFIG_FILE "$E/fonts.conf" \
    --setenv XXRI_DOCK_RESERVE "${XXRI_DOCK_RESERVE:-78}" \
    "$@" >>"$E/wm-app.log" 2>&1 &
  PIDS="$PIDS $!"     # never pkill -f here: the pattern matches this script
}
PIDS=""
: > "$E/wm-app.log"
# the window manager first, then the clients it has to decorate
DISPLAY=$DISP xsetroot -solid "#8b7fd8" 2>/dev/null
run "$WM"
sleep 1.5
for c in "$@"; do run $c; sleep 2.5; done
# optional interaction: ACTIONS holds xdotool commands, one per line, so the
# harness can prove hover / press / click behaviour and not just static looks
if [ -n "${ACTIONS:-}" ]; then
  printf '%s\n' "$ACTIONS" | while IFS= read -r a; do
    [ -n "$a" ] || continue
    DISPLAY=$DISP xdotool $a >/dev/null 2>&1
    sleep 0.4
  done
fi
sleep "${SETTLE:-2}"
DISPLAY=$DISP import -window root "$OUT" 2>/dev/null
echo "$OUT $(identify -format '%wx%h %b' "$OUT" 2>/dev/null)"
for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
kill $XP 2>/dev/null
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
