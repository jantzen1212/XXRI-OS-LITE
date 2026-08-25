# dock-diag-hook.sh - QA only: exercise the new dock plumbing from the guest.
(
  exec >/dev/ttyS0 2>&1
  sleep 28
  echo "=== DOCK DIAG ==="
  echo "wctl      : $(command -v xxri-wctl)  dock-launch: $(command -v xxri-dock-launch)"
  echo "DISPLAY   : ${DISPLAY:-unset}"
  echo "--- wctl list (nothing should be running yet) ---"
  DISPLAY=:0 xxri-wctl list 2>&1 | head -6
  echo "--- wctl activate on a program that is NOT running (expect non-zero) ---"
  DISPLAY=:0 xxri-wctl activate xxri-settings 2>&1; echo "  exit=$?"
  echo "--- run the dock command by hand ---"
  DISPLAY=:0 /usr/local/bin/xxri-dock-launch xxri-settings /usr/local/bin/xxri-settings 2>&1 &
  sleep 12
  echo "--- wctl list after the hand launch ---"
  DISPLAY=:0 xxri-wctl list 2>&1 | head -6
  echo "--- indicator daemon running? ---"
  ps 2>/dev/null | grep -c "[w]ctl indicators"
  echo "--- indicator daemon debug output ---"
  DISPLAY=:0 timeout 4 xxri-wctl indicators -d 2>&1 | head -6
  echo "=== END DOCK DIAG ==="
) &
