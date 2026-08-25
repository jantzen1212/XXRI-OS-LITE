# cc-trace-hook.sh - QA only: restart the Control Center with tracing on, then
# report what it saw while the pointer came and went.
(
  exec >/dev/ttyS0 2>&1
  sleep 26
  /usr/local/bin/xxri-control-center --quit 2>/dev/null
  sleep 2
  rm -f "$HOME/xxri-cc.log"
  XXRI_CC_DEBUG=1 DISPLAY=:0 /usr/local/bin/xxri-control-center &
  sleep 40
  echo "=== CC TRACE ==="
  tail -40 "$HOME/xxri-cc.log" 2>/dev/null || echo "no log"
  echo "=== END CC TRACE ==="
) &
