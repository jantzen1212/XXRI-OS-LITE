# geo-hook.sh - QA only: frame vs client geometry, to see whether the window is
# bigger than what the application paints.
(
  exec >/dev/ttyS0 2>&1
  sleep 30
  /usr/local/bin/xxri-settings >/dev/null 2>&1 &
  sleep 16
  echo "=== GEOMETRY ==="
  DISPLAY=:0 xwininfo -root -tree 2>/dev/null | grep -iE "settings|wbar" | head -6
  echo "--- detail ---"
  for w in $(DISPLAY=:0 xwininfo -root -tree 2>/dev/null | grep -i "xxri-settings" | sed -n 's/^ *\(0x[0-9a-f]*\).*/\1/p'); do
    echo "win $w:"
    DISPLAY=:0 xwininfo -id $w 2>/dev/null | grep -E "Absolute upper-left|Width:|Height:|Depth:|Border width"
  done
  echo "=== END GEOMETRY ==="
) &
