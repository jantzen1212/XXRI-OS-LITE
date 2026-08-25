# xcaps-hook.sh - QA only.  What does this X server actually offer, and is
# anything composited?  The answer decides whether real transparency is
# available or whether the desktop has to fake it.
(
  exec >/dev/ttyS0 2>&1
  sleep 26
  echo "=== X CAPABILITIES ==="
  echo "server   : $(pidof Xorg >/dev/null && echo X.Org || echo Xvesa)"
  echo "cmdline  : $(cat /proc/cmdline 2>/dev/null | tr ' ' '\n' | grep xres= || echo 'no xres=')"
  echo "--- extensions ---"
  DISPLAY=:0 xdpyinfo 2>/dev/null | sed -n '/number of extensions/,/^default screen/p' | head -30
  echo "--- screen / depth ---"
  DISPLAY=:0 xdpyinfo 2>/dev/null | grep -E "dimensions|depth of root|resolution"
  echo "32-bit ARGB visuals: $(DISPLAY=:0 xdpyinfo 2>/dev/null | grep -c 'depth: 32')"
  echo "--- compositing manager? ---"
  DISPLAY=:0 xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null || echo "  none"
  echo "--- root pixmap (pseudo-transparency source) ---"
  DISPLAY=:0 xprop -root _XROOTPMAP_ID 2>/dev/null || echo "  no _XROOTPMAP_ID"
  DISPLAY=:0 xprop -root ESETROOT_PMAP_ID 2>/dev/null || echo "  no ESETROOT_PMAP_ID"
  echo "--- xrandr ---"
  DISPLAY=:0 xrandr 2>/dev/null | head -10
  echo "=== END X CAPABILITIES ==="
) &
