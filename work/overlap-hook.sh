# overlap-hook.sh - QA only: open a terminal and the Store, then report exactly
# where every managed window is, so the host can sample a pixel that is
# genuinely inside the Store's translucent rail AND on top of the terminal.
(
  exec >/dev/ttyS0 2>&1
  sleep 30
  aterm >/dev/null 2>&1 &
  sleep 6
  /usr/local/bin/xxri-store-gui >/dev/null 2>&1 &
  sleep 24
  echo "=== OVERLAP ==="
  DISPLAY=:0 xwininfo -root -tree 2>/dev/null | sed -n 's/^ *\(0x[0-9a-f]*\) "\([^"]*\)".*  \([0-9]*x[0-9]*+[0-9-]*+[0-9-]*\).*/\1 \2 \3/p' | head -8
  echo "=== END OVERLAP ==="
) &
