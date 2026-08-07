( LOG="$HOME/e95.log"; DOCK=/usr/local/tce.icons; MENU="$HOME/.wmx/Applications"
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  echo "=== Phase 9.5 device verification $(date) ===" > "$LOG"
  echo "bash: $(command -v bash || echo MISSING)" >> "$LOG"
  ok=0; n=0
  for ID in appimagetool sponge256sum mangbandclient python fre-ac windows2usb; do
    n=$((n+1)); echo "" >> "$LOG"; echo "--- [$n] $ID ---" >> "$LOG"
    xxri-store install "$ID" >> "$LOG" 2>&1
    reg="$(xxri-app info "$ID" 2>/dev/null)"; nm="$(echo "$reg" | sed -n 's/^NAME=//p')"
    echo "  $nm $(echo "$reg" | sed -n 's/^VERSION=//p') | dock:$(grep -c "exec xxri-app launch $ID" $DOCK 2>/dev/null) menu:$({ m=$(echo "$nm"|tr -d ' '); [ -f "$MENU/$m" ] && echo yes || echo NO; })" >> "$LOG"
    xxri-app launch "$ID" >> "$LOG" 2>&1; sleep 7
    rc="$(grep "exit .* for .*$ID" /tmp/xxri-app.log 2>/dev/null | tail -1 | sed 's/.*exit \([0-9]*\) .*/\1/')"
    if [ -z "$rc" ] || [ "$rc" = 0 ]; then echo "  launch: OK (rc=${rc:-running})" >> "$LOG"; ok=$((ok+1))
    else echo "  launch: FAILED rc=$rc" >> "$LOG"; fi
    pkill -f "$ID" 2>/dev/null
  done
  echo "" >> "$LOG"; xxri-app list >> "$LOG" 2>&1
  echo "RESULT: $ok/$n launched cleanly" >> "$LOG"; echo "=== E95 DONE ===" >> "$LOG"; sync
) >/dev/null 2>&1 &
