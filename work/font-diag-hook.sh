# font-diag-hook.sh - QA only.  Answers, from the guest itself, why the window
# manager's titlebar is drawing in a bitmap fallback instead of the XXRI face.
(
  exec >/dev/ttyS0 2>&1
  sleep 20
  echo "=== XXRI FONT DIAG ==="
  echo "XSERVER guess : $(pidof Xorg >/dev/null && echo xorg || echo xvesa)"
  echo "xset present  : $(command -v xset || echo MISSING)"
  echo "font dir      : $(ls -d /usr/local/share/fonts/xxri 2>/dev/null || echo MISSING)"
  echo "fonts.dir line: $(head -2 /usr/local/share/fonts/xxri/fonts.dir 2>/dev/null | tail -1)"
  echo "--- what .xsession's own xset did ---"
  cat /tmp/xset.log 2>/dev/null | head -12 || echo "no /tmp/xset.log"
  echo "--- current font path ---"
  DISPLAY=:0 xset q 2>&1 | sed -n '/Font Path/,+3p'
  echo "--- try adding it by hand ---"
  DISPLAY=:0 xset +fp /usr/local/share/fonts/xxri/ 2>&1
  DISPLAY=:0 xset fp rehash 2>&1
  DISPLAY=:0 xset q 2>&1 | sed -n '/Font Path/,+3p'
  echo "--- can the server serve the XXRI face? ---"
  if command -v xlsfonts >/dev/null; then
    DISPLAY=:0 xlsfonts -fn '-xxri-*' 2>&1 | head -5
  else
    echo "xlsfonts MISSING"
  fi
  echo "--- xorg.log font complaints ---"
  grep -iE "font|FreeType|Type1" /tmp/xorg.log 2>/dev/null | tail -12
  echo "=== END FONT DIAG ==="
) &
