#!/bin/bash
# wm-stress.sh - hammer the XXRI window manager the way a user does, with a
# REAL ~/.wmx tree, and report whether it is still alive at every step.
#
# The previous crash only appeared once ~/.wmx/Applications held real
# launchers, so an empty harness proves nothing.  This one reproduces the
# device's own tree (8 Applications launchers + an empty SystemTools submenu,
# read off the installed image with debugfs) and then does what a user does:
# open the desktop menu, open windows, minimise, restore, maximise, close, and
# open the menu again.  glibc heap checking is on, so a heap overwrite aborts
# at the next free() instead of silently corrupting memory.
#
#   WM=<binary>   which window manager to test (default /tmp/claude-1000/flwm-xxri)
#   ROUNDS=<n>    how many menu/window cycles (default 6)
set -u
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
WM="${WM:-/tmp/claude-1000/flwm-xxri}"; DISP="${DISP:-:97}"; ROUNDS="${ROUNDS:-6}"
H="$E/stresshome"
OUT="${OUT:-/tmp/claude-1000/wmstress}"; mkdir -p "$OUT" "$E/run"; chmod 700 "$E/run"

# ---- a real ~/.wmx, exactly like the device's -------------------------------
rm -rf "$H"; mkdir -p "$H/.wmx/Applications" "$H/.wmx/SystemTools"
mk() { printf '#!/bin/sh\nexec %s\n' "$2" > "$H/.wmx/Applications/$1"; chmod 755 "$H/.wmx/Applications/$1"; }
mk Terminal      "aterm"
mk ControlCenter "/usr/local/bin/xxri-control-center"
mk Disks         "/usr/local/bin/xxri-settings storage"
mk TextEditor    "editor -scheme gtk+"
mk Power         "/usr/local/bin/xxri-power-menu"
mk Screenshot    "/usr/local/bin/xxri-screenshot"
mk XXRISettings  "/usr/local/bin/xxri-settings"
mk XXRIStore     "/usr/local/bin/xxri-store-gui"

ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
sleep 0.4
Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp \
     -fp "$R/usr/local/share/fonts/xxri/,built-ins" >"$E/wms-xvfb.log" 2>&1 &
XP=$!
for i in $(seq 1 50); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done

PIDS=""
run() {
  bwrap --ro-bind / / --tmpfs /usr/local/share \
    --ro-bind "$R/usr/local/share/xxri-settings" /usr/local/share/xxri-settings \
    --ro-bind "$R/usr/local/share/xxri-store" /usr/local/share/xxri-store \
    --ro-bind "$R/usr/local/share/pixmaps" /usr/local/share/pixmaps \
    --ro-bind "$R/usr/local/share/fonts" /usr/local/share/fonts \
    --bind "$E" "$E" --dev /dev --proc /proc \
    --setenv LD_LIBRARY_PATH "$L/lib" --setenv XDG_DATA_DIRS "$L/share:/usr/share" \
    --setenv PATH "$E/stubs:/usr/bin:/bin" --setenv DISPLAY "$DISP" \
    --setenv GDK_BACKEND x11 --setenv NO_AT_BRIDGE 1 --setenv GTK_A11Y none \
    --setenv GDK_GL disable --setenv XDG_RUNTIME_DIR "$E/run" \
    --setenv FONTCONFIG_FILE "$E/fonts.conf" --setenv XXRI_DOCK_RESERVE 78 \
    --setenv MALLOC_CHECK_ 3 --setenv MALLOC_PERTURB_ 165 \
    --setenv HOME "$H" "$@" >>"$E/wms-app.log" 2>&1 &
  PIDS="$PIDS $!"
}
: > "$E/wms-app.log"; : > "$E/wms-wm.log"
DISPLAY=$DISP xsetroot -solid "#8b7fd8" 2>/dev/null

# the window manager, with its stderr kept separate so an abort is legible
bwrap --ro-bind / / --tmpfs /usr/local/share \
  --ro-bind "$R/usr/local/share/pixmaps" /usr/local/share/pixmaps \
  --ro-bind "$R/usr/local/share/fonts" /usr/local/share/fonts \
  --bind "$E" "$E" --dev /dev --proc /proc \
  --setenv LD_LIBRARY_PATH "$L/lib" --setenv PATH "$E/stubs:/usr/bin:/bin" \
  --setenv DISPLAY "$DISP" --setenv XDG_RUNTIME_DIR "$E/run" \
  --setenv FONTCONFIG_FILE "$E/fonts.conf" --setenv XXRI_DOCK_RESERVE 78 \
  --setenv MALLOC_CHECK_ 3 --setenv MALLOC_PERTURB_ 165 \
  --setenv HOME "$H" "$WM" >"$E/wms-wm.log" 2>&1 &
WMPID=$!
sleep 2

WMNAME=$(basename "$WM")
alive() { pgrep -x "$WMNAME" >/dev/null && echo up || echo "DOWN"; }
x() { DISPLAY=$DISP xdotool "$@" >/dev/null 2>&1; }
step() {  # step LABEL - report and bail out the moment the WM dies
  local s; s=$(alive)
  printf '  %-34s wm=%s\n' "$1" "$s"
  [ "$s" = up ] || { echo "*** WINDOW MANAGER DIED at: $1"; DEAD=1; }
}
DEAD=0
menu() {  # open the desktop menu on the root window and dismiss it
  x mousemove 640 420; x mousedown 1; sleep 0.7
  x mousemove 660 440; sleep 0.3; x mouseup 1; sleep 0.6
  x key Escape; sleep 0.4
}

echo "== wm=$WM  rounds=$ROUNDS  wmx entries=$(find "$H/.wmx" -mindepth 1 | wc -l)"
step "startup"
for r in $(seq 1 "$ROUNDS"); do
  [ "$DEAD" = 1 ] && break
  echo "-- round $r"
  menu;                                          step "menu open/close (no client)"
  run "$R/usr/local/bin/xxri-settings" about; sleep 3.5
  step "settings mapped"
  menu;                                          step "menu with 1 client"
  # minimise with the XXRI control, then open the menu (the old crash path)
  x mousemove 18 13 click 1; sleep 1.2;          step "minimise"
  menu;                                          step "menu with iconic client"
  # restore through the menu
  x mousemove 640 420 mousedown 1; sleep 0.8
  x mousemove 660 445; sleep 0.4; x mouseup 1; sleep 1.2
  step "restore from menu"
  x mousemove 33 13 click 1; sleep 1.0;          step "maximise"
  x mousemove 33 13 click 1; sleep 1.0;          step "restore"
  x mousemove 300 13 mousedown 1; sleep 0.3
  for i in 1 2 3 4 5 6; do x mousemove $((300+i*20)) $((13+i*12)); sleep 0.08; done
  x mouseup 1; sleep 0.6;                        step "drag titlebar"
  menu;                                          step "menu after drag"
  x mousemove 48 13 click 1; sleep 1.5;          step "close"
  menu;                                          step "menu after close"
done

DISPLAY=$DISP import -window root "$OUT/final.png" 2>/dev/null
echo "== final: wm=$(alive)"
echo "--- wm stderr ---"; tail -15 "$E/wms-wm.log"
for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
kill -9 $WMPID 2>/dev/null
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
