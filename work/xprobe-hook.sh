# xprobe-hook.sh - QA only: ask the real X server what it can composite.
(
  exec >/dev/ttyS0 2>&1
  sleep 28
  echo "=== XPROBE ==="
  DISPLAY=:0 /usr/local/bin/xprobe 2>&1
  echo "--- idle ---"
  cat /proc/loadavg
  free 2>/dev/null | head -2
  echo "=== END XPROBE ==="
) &
