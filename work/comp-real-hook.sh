# comp-real-hook.sh - QA only: read the compositor's OWN log from the real
# session start, without restarting it (restarting perturbs what we measure).
(
  exec >/dev/ttyS0 2>&1
  sleep 44
  echo "=== REAL COMP ==="
  echo "screen : $(DISPLAY=:0 xdpyinfo 2>/dev/null | awk '/dimensions:/{print $2}')"
  echo "cc     : $(ps 2>/dev/null | grep -c '[x]xri-control-cente')"
  grep -E "^screen|^owning" /tmp/compositor.log 2>/dev/null
  echo "--- adds ---"; grep "^add 0x" /tmp/compositor.log | tail -12
  echo "--- last paint cycle ---"; grep -A8 "^paint:" /tmp/compositor.log | tail -9
  echo "=== END REAL COMP ==="
) &
