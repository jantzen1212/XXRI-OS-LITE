# perf-hook.sh - QA only: what the desktop costs, with whatever is running.
(
  exec >/dev/ttyS0 2>&1
  sleep 40
  cpu() { grep '^cpu ' /proc/stat | awk '{u=$2+$3+$4; t=u+$5; print u" "t}'; }
  busy() { local a b; a=$(cpu); sleep "$1"; b=$(cpu)
           echo "$a $b" | awk '{du=$3-$1; dt=$4-$2; if(dt>0) printf "%.1f%%", 100*du/dt; else print "?"}'; }
  echo "=== PERF ==="
  echo "compositor    : $(pidof xxri-compositor >/dev/null && echo ON || echo off)"
  echo "idle CPU (8s) : $(busy 8)"
  echo "RAM used      : $(free 2>/dev/null | awk '/^Mem/{print $3" KB"}')"
  echo "comp RSS      : $(awk '/VmRSS/{print $2" KB"}' /proc/$(pidof xxri-compositor 2>/dev/null)/status 2>/dev/null || echo n/a)"
  echo "SHOTREADY-PERF"
  sleep 6
  echo "CPU during drag: measured by the host next"
  echo "=== END PERF ==="
) &
