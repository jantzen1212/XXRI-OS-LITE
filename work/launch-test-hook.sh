# Launch each installed app and record its real exit code (persistently).
( LOG="$HOME/launchtest.log"
  i=0; while [ $i -lt 40 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  echo "=== launch exit-code test $(date) ===" > "$LOG"
  for ID in basiliskii fre-ac zsync2; do
    f="$(xxri-app info "$ID" 2>/dev/null | sed -n 's/^FILE=//p')"
    [ -n "$f" ] || { echo "$ID: not installed" >> "$LOG"; continue; }
    # run the AppImage exactly as xxri-app does, but capture the code ourselves
    out="$(XXRI_APP_QUIET=1 timeout 25 xxri-app launch "$ID" 2>&1)"
    sleep 8
    rc="$(grep "exit .* for .*${ID}" /tmp/xxri-app.log 2>/dev/null | tail -1 | sed 's/.*exit \([0-9]*\) .*/\1/')"
    [ -n "$rc" ] || rc="(still running)"
    echo "$ID: exit=$rc" >> "$LOG"
    pkill -f "$ID" 2>/dev/null
  done
  echo "--- xxri-app log tail ---" >> "$LOG"
  tail -25 /tmp/xxri-app.log >> "$LOG" 2>&1
  echo "=== LT DONE ===" >> "$LOG"; sync
) >/dev/null 2>&1 &
