( LOG="$HOME/mainstream.log"; DOCK=/usr/local/tce.icons; MENU="$HOME/.wmx/Applications"
  APPS="${XXRI_TEST_APPS:-gimp inkscape vlc audacity geany}"
  SHOW="${XXRI_SHOW_SECS:-26}"
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  echo "=== mainstream install test $(date) ===" > "$LOG"
  echo "apps: $APPS" >> "$LOG"
  for ID in $APPS; do
    echo "" >> "$LOG"; echo "--- $ID ---" >> "$LOG"
    t0=$(date +%s)
    xxri-store install "$ID" >> "$LOG" 2>&1
    echo "  install took $(( $(date +%s) - t0 ))s" >> "$LOG"
    reg="$(xxri-app info "$ID" 2>/dev/null)"
    if [ -z "$reg" ]; then echo "  NOT REGISTERED - install failed" >> "$LOG"; continue; fi
    nm="$(echo "$reg" | sed -n 's/^NAME=//p')"; fl="$(echo "$reg"|sed -n 's/^FILE=//p')"
    echo "  NAME=$nm KIND=$(echo "$reg"|sed -n 's/^KIND=//p') FILE=$fl" >> "$LOG"
    echo "  desktop:$([ -f "$(echo "$reg"|sed -n 's/^DESKTOP=//p')" ] && echo yes || echo NO)" \
         "dock:$(grep -c "exec xxri-app launch $ID" $DOCK 2>/dev/null)" \
         "menu:$({ m=$(echo "$nm"|tr -d ' '); [ -f "$MENU/$m" ] && echo yes || echo NO; })" \
         "exec:$([ -x "$fl" ] && echo yes || echo NO)" >> "$LOG"
    echo "  launching..." >> "$LOG"
    xxri-app launch "$ID" >> "$LOG" 2>&1
    # give it time to map a window, then hold it on screen for the host screendump
    sleep "$SHOW"
    rc="$(grep "exit .* for .*$ID" /tmp/xxri-app.log 2>/dev/null | tail -1 | sed 's/.*exit \([0-9]*\) .*/\1/')"
    if [ -z "$rc" ]; then echo "  RUNNING (no exit recorded)" >> "$LOG"
    else echo "  EXITED rc=$rc" >> "$LOG"; fi
    # whether or not it exited, record what it printed and whether it ever
    # mapped a window - "running but invisible" and "died instantly" look the
    # same from the outside otherwise.
    echo "  --- app output ---" >> "$LOG"
    tail -20 /tmp/xxri-app.log >> "$LOG" 2>&1
    echo "  --- toplevel windows ---" >> "$LOG"
    xwininfo -root -children 2>/dev/null | grep -vE '^\s*$|root window|children:' | tail -12 >> "$LOG"
    echo "SHOT:$ID" >> "$LOG"; sync
    sleep 8
    # close the window so the next app gets a clean screen
    p="$(ps -eo pid,args 2>/dev/null | grep -v grep | grep -i "$fl" | awk '{print $1}' | head -1)"
    [ -n "$p" ] && { kill "$p" 2>/dev/null; sleep 3; kill -9 "$p" 2>/dev/null; }
    sleep 3
  done
  echo "" >> "$LOG"; echo "=== registry ===" >> "$LOG"
  xxri-app list >> "$LOG" 2>&1
  echo "=== dock app entries: $(grep -c '^c: exec xxri-app launch' $DOCK 2>/dev/null) ===" >> "$LOG"
  echo "=== MS DONE ===" >> "$LOG"; sync
) >/dev/null 2>&1 &
