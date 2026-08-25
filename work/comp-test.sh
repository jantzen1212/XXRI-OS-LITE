#!/bin/bash
# comp-test.sh - does the compositor actually composite?  Runs the real WM, the
# compositor and a real client under Xvfb, then shoots the screen.
set -u
E=~/.xxri-testenv; SYS=$E/sys; L=$SYS/usr/local; R=/home/jantzen/xxri-build/rootfs
DISP="${DISP:-:90}"; C="${C:-/tmp/claude-1000/xxri-compositor}"
WM="${WM:-/tmp/claude-1000/flwm-anim}"; O=/tmp/claude-1000
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
sleep 0.4
Xvfb "$DISP" -screen 0 1024x768x24 +extension Composite +extension DAMAGE \
     -nolisten tcp -fp "$R/usr/local/share/fonts/xxri/,built-ins" >"$E/comp-xvfb.log" 2>&1 &
XP=$!
for i in $(seq 1 60); do DISPLAY=$DISP xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
echo "extensions: $(DISPLAY=$DISP xdpyinfo 2>/dev/null | grep -cE '^    (Composite|DAMAGE|XFIXES|RENDER)')/4"
mkdir -p "$E/run"; chmod 700 "$E/run"
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
    --setenv HOME "$E" "$@" >>"$E/comp-app.log" 2>&1 &
  PIDS="$PIDS $!"
}
: > "$E/comp-app.log"; : > "$E/comp.log"
DISPLAY=$DISP xsetroot -solid "#6a5acd"
run "$WM"; sleep 1.5
run "$R/usr/local/bin/xxri-settings" about; sleep 4
DISPLAY=$DISP import -window root "$O/comp-before.png" 2>/dev/null
echo "before compositor: $(identify -format '%wx%h mean=%[fx:mean]' $O/comp-before.png 2>/dev/null)"
run "$C"; sleep 3
DISPLAY=$DISP import -window root "$O/comp-after.png" 2>/dev/null
echo "with compositor  : $(identify -format '%wx%h mean=%[fx:mean]' $O/comp-after.png 2>/dev/null)"
echo "compositor alive : $(pgrep -x xxri-compositor >/dev/null && echo yes || echo NO)"
# the real question: does an ARGB window blend?
run /tmp/claude-1000/argbtest; sleep 3
DISPLAY=$DISP import -window root "$O/comp-argb.png" 2>/dev/null
echo "ARGB window      : $(identify -format '%wx%h' $O/comp-argb.png 2>/dev/null)"
echo "  pixel inside it: $(magick $O/comp-argb.png -crop 1x1+500+300 +repage txt: | tail -1 | grep -o '#[0-9A-F]*')"
echo "  same spot before: $(magick $O/comp-after.png -crop 1x1+500+300 +repage txt: | tail -1 | grep -o '#[0-9A-F]*')"
# per-window opacity (secondary)
CID=$(DISPLAY=$DISP xdotool search --name "XXRI Settings" 2>/dev/null | tail -1)
FID=$(DISPLAY=$DISP xwininfo -id "$CID" 2>/dev/null | awk '/Parent window id/{print $4}')
echo "client=$CID frame=$FID"
for w in $CID $FID; do
  DISPLAY=$DISP xprop -id "$w" -f _NET_WM_WINDOW_OPACITY 32c \
      -set _NET_WM_WINDOW_OPACITY 2576980377 2>/dev/null   # 0.60
done
sleep 2
DISPLAY=$DISP import -window root "$O/comp-opacity.png" 2>/dev/null
echo "at 60% opacity   : $(identify -format '%wx%h mean=%[fx:mean]' $O/comp-opacity.png 2>/dev/null)"
echo "--- compositor stderr ---"; tail -5 "$E/comp-app.log" | grep -i composit || echo "(none)"
for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
kill -9 $XP 2>/dev/null
ps -eo pid,args | awk -v d="Xvfb $DISP" '$0 ~ d && $2 ~ /Xvfb$/ {print $1}' | xargs -r kill -9 2>/dev/null
exit 0
