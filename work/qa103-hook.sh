# qa103-hook.sh - QA only: report the things Phase 10.3 must not have broken.
(
  exec >/dev/ttyS0 2>&1
  sleep 30
  echo "=== QA103 ==="
  echo "wm        : $(pidof flwm >/dev/null && echo flwm-up || echo DOWN) $(readlink -f /usr/local/bin/flwm)"
  echo "dock      : $(pidof wbar >/dev/null && echo wbar-up || echo DOWN)"
  echo "indicators: $(ps 2>/dev/null | grep -c '[w]ctl indicators')"
  echo "cc        : $(ps 2>/dev/null | grep -c '[x]xri-control-cente')"
  echo "dock slots: $(grep -c '^c: exec' /usr/local/tce.icons)"
  echo "--- icon integrity (Phase 9 checker) ---"
  if [ -x /usr/local/bin/xxri-app ]; then /usr/local/bin/xxri-app verify-icons 2>&1 | tail -5; else echo "xxri-app MISSING"; fi
  echo "--- dock icon files present ---"
  for i in xxri-store xxri-settings mnttool aterm editor exittc; do
    f=/usr/local/share/pixmaps/$i.png
    printf "  %-14s %s\n" "$i" "$([ -s "$f" ] && echo "$(wc -c < "$f") bytes" || echo MISSING)"
  done
  echo "--- desktop entries (none may be Tiny Core named) ---"
  ls /usr/local/share/applications/*.desktop 2>/dev/null | wc -l
  grep -lis "tiny core\|tinycore" /usr/local/share/applications/*.desktop 2>/dev/null | wc -l
  echo "--- X server + font path ---"
  DISPLAY=:0 xset q 2>/dev/null | sed -n '/Font Path/,+1p' | tail -1
  echo "=== END QA103 ==="
) &
