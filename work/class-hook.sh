# class-hook.sh - QA only: report WM_CLASS for every managed client, so the dock
# helper can be built on a real matching key rather than a guess.
(
  exec >/dev/ttyS0 2>&1
  sleep 30
  echo "=== WM_CLASS PROBE ==="
  for w in $(DISPLAY=:0 xwininfo -root -tree 2>/dev/null | sed -n 's/^ *\(0x[0-9a-f]*\) .*/\1/p'); do
    cls=$(DISPLAY=:0 xprop -id $w WM_CLASS 2>/dev/null | sed -n 's/WM_CLASS(STRING) = //p')
    st=$(DISPLAY=:0 xprop -id $w WM_STATE 2>/dev/null | sed -n 's/.*window state: *//p')
    [ -n "$cls" ] && echo "  $w state=${st:-none} class=$cls"
  done
  echo "=== END WM_CLASS PROBE ==="
) &
