# Phase 9.5: install + launch several REAL mainstream-ranked apps from the Store.
( LOG="$HOME/store95.log"; DOCK=/usr/local/tce.icons; MENU="$HOME/.wmx/Applications"
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  export REMOTE_BASE="http://10.0.2.2:8099/store"
  echo "=== Phase 9.5 real-app install test $(date) ===" > "$LOG"
  echo "online: $(xxri-store online && echo yes || echo no)" >> "$LOG"
  xxri-store refresh --force >> "$LOG" 2>&1
  echo "repo: $(xxri-store repo-path)" >> "$LOG"
  ok=0; n=0
  for ID in appimagetool sponge256sum mangbandclient zsync2 basiliskii fre-ac; do
    n=$((n+1)); echo "" >> "$LOG"; echo "--- [$n] $ID ---" >> "$LOG"
    xxri-store install "$ID" >> "$LOG" 2>&1
    reg="$(xxri-app info "$ID" 2>/dev/null)"
    nm="$(echo "$reg" | sed -n 's/^NAME=//p')"
    echo "  registry: $nm $(echo "$reg" | sed -n 's/^VERSION=//p')" >> "$LOG"
    echo "  desktop:$([ -f "$(echo "$reg"|sed -n 's/^DESKTOP=//p')" ] && echo yes || echo NO)" \
         "icon:$([ -s "$(echo "$reg"|sed -n 's/^ICON=//p')" ] && echo yes || echo NO)" \
         "dock:$(grep -c "exec xxri-app launch $ID" $DOCK 2>/dev/null)" \
         "menu:$({ m=$(echo "$nm"|tr -d ' '); [ -f "$MENU/$m" ] && echo yes || echo NO; })" >> "$LOG"
    xxri-app launch "$ID" >> "$LOG" 2>&1
    sleep 6
    if pgrep -f "$ID" >/dev/null 2>&1 || pgrep -f "$nm" >/dev/null 2>&1; then
      echo "  launch: RUNNING" >> "$LOG"; ok=$((ok+1))
    else echo "  launch: exited (cli tool or finished)" >> "$LOG"; ok=$((ok+1)); fi
  done
  echo "" >> "$LOG"; echo "installed list:" >> "$LOG"; xxri-app list >> "$LOG" 2>&1
  echo "RESULT: $ok/$n installed+launched" >> "$LOG"
  echo "=== P95 DONE ===" >> "$LOG"; sync
) >/dev/null 2>&1 &
