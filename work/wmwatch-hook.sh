# wmwatch-hook.sh - QA only.  Streams the window tree to the serial port so an
# interaction test can be read as numbers: frame rectangle, client rectangle,
# and therefore whether a decoration is present at all.
(
  exec >/dev/ttyS0 2>&1
  i=0; while [ $i -lt 40 ]; do route -n 2>/dev/null | grep -q '^0\.0\.0\.0' && break; sleep 2; i=$((i+2)); done
  echo "=== wmwatch  wm=$(pidof flwm >/dev/null && echo up || echo DOWN)"
  n=0
  while [ $n -lt 200 ]; do
    sleep 5; n=$((n+1))
    echo "--- t=$((n*5))s wm=$(pidof flwm >/dev/null && echo up || echo DOWN)"
    DISPLAY=:0 xwininfo -root -tree 2>/dev/null | awk '
      /^ +0x/ && /[0-9]+x[0-9]+\+/ {
        g=""; for(i=1;i<=NF;i++) if ($i ~ /^[0-9]+x[0-9]+\+/) g=$i;
        nm=""; for(i=2;i<=NF;i++){ if ($i ~ /^\(/) break; nm=nm" "$i }
        if (g!="" && g !~ /^1x1/ && g !~ /^10x10/ && g !~ /^5x5/)
          printf "   %-28s %s\n", substr(nm,2,28), g }'
  done
) &
