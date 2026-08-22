( LOG="$HOME/el.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  say "=== Electron window-mapping diagnosis $(date) ==="
  say "  X server : ${XSERVER:-?}   DISPLAY=$DISPLAY"
  say "  WM       : $(ps -eo comm | grep -cE '^flwm') flwm process(es)"
  say "  EWMH     : $(xprop -root _NET_SUPPORTING_WM_CHECK 2>&1 | head -1)"
  say "  _NET_WM  : $(xprop -root _NET_SUPPORTED 2>&1 | head -c 160)"
  say ""
  say "--- baseline: windows on the root BEFORE launching ---"
  xwininfo -root -children 2>/dev/null | sed -n '/children:/,$p' | head -12 | sed 's/^/    /' >> "$LOG"

  B=/opt/xxri/code-oss/VSCode-linux-ia32/code
  [ -x "$B" ] || { xxri-store install code-oss >/dev/null 2>&1; }
  [ -x "$B" ] || { say "  code-oss not installed"; echo DONE > "$HOME/stage"; exit; }

  say ""
  say "--- launching with Electron logging enabled ---"
  echo "code-oss" > "$HOME/stage"; sync
  ( cd "$HOME"; timeout 150 "$B" --no-sandbox --disable-gpu --enable-logging=stderr --v=1 ) >/tmp/el.out 2>&1 &
  sleep 45

  say "  processes: $(ps -eo args | grep -c 'VSCode-linux-ia32/code')"
  say ""
  say "--- STAGE TEST: does an X window exist at all? ---"
  xwininfo -root -children 2>/dev/null | sed -n '/children:/,$p' | head -20 | sed 's/^/    /' >> "$LOG"
  say ""
  say "--- _NET_CLIENT_LIST (what the WM is managing) ---"
  xprop -root _NET_CLIENT_LIST 2>&1 | head -3 | sed 's/^/    /' >> "$LOG"
  say ""
  say "--- any window whose class mentions code/electron ---"
  for w in $(xwininfo -root -children 2>/dev/null | awk '/0x/{print $1}'); do
      cls=$(xprop -id "$w" WM_CLASS 2>/dev/null | head -1)
      case "$cls" in *ode*|*lectron*|*hrom*)
          say "    $w  $cls"
          xwininfo -id "$w" 2>/dev/null | grep -E 'Map State|Width|Height|Override' | sed 's/^/      /' >> "$LOG" ;;
      esac
  done
  say ""
  say "--- Electron stderr: window / X11 / GPU lines ---"
  grep -iE 'window|x11|xlib|gpu|glx|egl|display|crash|sandbox|dbus|fatal' /tmp/el.out 2>/dev/null | head -25 | sed 's/^/    /' >> "$LOG"
  say ""
  say "--- last 12 lines of Electron output ---"
  tail -12 /tmp/el.out 2>/dev/null | sed 's/^/    /' >> "$LOG"
  sleep 30
  say ""
  say "--- after 75s total: still alive? ---"
  say "  processes: $(ps -eo args | grep -c 'VSCode-linux-ia32/code')"
  xwininfo -root -children 2>/dev/null | grep -ciE 'code|electron' | sed 's/^/  code-ish windows: /' >> "$LOG"
  echo DONE > "$HOME/stage"
  say "=== EL DONE ==="; sync
) >/dev/null 2>&1 &
