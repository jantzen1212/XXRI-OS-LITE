# wlist-hook.sh - QA only: stream the managed-window list, via the same helper
# the dock uses, so the test can assert on state instead of on pixels.
(
  exec >/dev/ttyS0 2>&1
  sleep 22
  n=0
  while [ $n -lt 250 ]; do
    n=$((n+1))
    echo "WL $(DISPLAY=:0 xxri-wctl list 2>/dev/null | awk '{printf "%s(%s) ", $3, $2}')"
    sleep 3
  done
) &
