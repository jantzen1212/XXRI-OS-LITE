# argb-only-hook.sh - QA only: an ARGB window with NO compositor running.
# Without a compositor its alpha is meaningless, but an OPAQUE region must still
# paint - which tells us whether the test client itself works.
(
  exec >/dev/ttyS0 2>&1
  sleep 30
  echo "=== ARGB WITHOUT COMPOSITOR ==="
  DISPLAY=:0 /usr/local/bin/argbtest >/tmp/argb.log 2>&1 &
  sleep 4
  echo "running : $(pidof argbtest >/dev/null && echo yes || echo NO)"
  echo "stderr  : $(head -3 /tmp/argb.log 2>/dev/null)"
  echo "SHOTREADY"
  sleep 20
  echo "=== END ==="
) &
