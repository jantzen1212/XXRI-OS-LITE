# res-diag-hook.sh - QA only: why is the Control Center missing at this size?
(
  exec >/dev/ttyS0 2>&1
  sleep 34
  echo "=== RES DIAG ==="
  echo "screen     : $(DISPLAY=:0 xdpyinfo 2>/dev/null | awk '/dimensions:/{print $2}')"
  echo "compositor : $(pidof xxri-compositor >/dev/null && echo up || echo DOWN)"
  echo "cc         : $(ps 2>/dev/null | grep -c '[x]xri-control-cente')"
  echo "cc stderr  : $(head -3 /tmp/compositor.log 2>/dev/null)"
  echo "--- windows the server has ---"
  DISPLAY=:0 xwininfo -root -children 2>/dev/null | tail -12
  echo "=== END RES DIAG ==="
) &
