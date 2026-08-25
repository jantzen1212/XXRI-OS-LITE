# comp-dbg-hook.sh - QA only: restart the compositor with tracing and report
# whether it finds an ARGB client to adopt.
(
  exec >/dev/ttyS0 2>&1
  sleep 26
  killall xxri-compositor 2>/dev/null; pkill -x xxri-compositor 2>/dev/null
  sleep 2
  XXRI_COMP_DEBUG=1 DISPLAY=:0 /usr/local/bin/xxri-compositor >/tmp/compdbg.log 2>&1 &
  sleep 40
  echo "=== COMP DBG ==="
  echo "compositor : $(pidof xxri-compositor >/dev/null && echo up || echo DOWN)"
  echo "--- adopt / selection ---"
  grep -i "adopt\|owning" /tmp/compdbg.log | head -5
  echo "--- windows added ---"
  grep "^add 0x" /tmp/compdbg.log | tail -10
  echo "--- paint lines (last cycle) ---"
  grep -A6 "^paint:" /tmp/compdbg.log | tail -8
  echo "--- raw log, first 30 lines ---"
  head -30 /tmp/compdbg.log
  echo "=== END COMP DBG ==="
) &
