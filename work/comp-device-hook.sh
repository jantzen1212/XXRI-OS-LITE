# comp-device-hook.sh - QA only: start the compositor on the real target, prove
# an ARGB window blends, and measure what it costs.
(
  exec >/dev/ttyS0 2>&1
  sleep 30
  cpu() { grep '^cpu ' /proc/stat | awk '{u=$2+$3+$4; t=u+$5; print u" "t}'; }
  busy() { # percent busy between two samples, over ~4s
    local a b; a=$(cpu); sleep 4; b=$(cpu)
    echo "$a $b" | awk '{du=$3-$1; dt=$4-$2; if(dt>0) printf "%.1f%%", 100*du/dt; else print "?"}'
  }
  echo "=== COMPOSITOR ON TARGET ==="
  echo "idle CPU, no compositor : $(busy)"
  echo "mem before              : $(free 2>/dev/null | awk '/^Mem/{print $3" KB used"}')"
  XXRI_COMP_DEBUG=1 DISPLAY=:0 /usr/local/bin/xxri-compositor >/tmp/comp.log 2>&1 &
  sleep 4
  echo "compositor running      : $(pidof xxri-compositor >/dev/null && echo yes || echo NO)"
  echo "compositor stderr       : $(head -2 /tmp/comp.log 2>/dev/null)"
  echo "idle CPU, compositing   : $(busy)"
  echo "mem after               : $(free 2>/dev/null | awk '/^Mem/{print $3" KB used"}')"
  echo "compositor RSS          : $(awk '/VmRSS/{print $2" KB"}' /proc/$(pidof xxri-compositor)/status 2>/dev/null)"
  echo "--- now an ARGB window ---"
  DISPLAY=:0 /usr/local/bin/argbtest >/tmp/argb.log 2>&1 &
  sleep 3
  echo "argbtest running        : $(pidof argbtest >/dev/null && echo yes || echo "NO: $(cat /tmp/argb.log 2>/dev/null)")"
  echo "SHOTREADY-ARGB"
  sleep 12
  echo "CPU while ARGB repaints : $(busy)"
  echo "--- compositor log (tail) ---"
  tail -22 /tmp/comp.log 2>/dev/null
  echo "=== END COMPOSITOR ==="
) &
