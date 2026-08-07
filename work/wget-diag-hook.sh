( LOG="$HOME/wgetdiag.log"
  i=0; while [ $i -lt 40 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  { echo "=== wget/TLS diagnostic ==="
    echo "route wait: ${i}s ; gw=$(route -n 2>/dev/null | awk '$1=="0.0.0.0"{print $2;exit}')"
    echo "which wget : $(command -v wget)"
    echo "gnu wget   : $(/usr/local/bin/wget --version 2>&1 | head -1)"
    echo "CA bundle  : $(ls -l /usr/local/etc/ssl/certs/ca-certificates.crt 2>/dev/null | awk '{print $5" bytes"}' || echo MISSING)"
    echo
    echo "--- GNU wget https (verbose stderr):"
    /usr/local/bin/wget --timeout=25 --tries=1 -O /tmp/gh.json https://api.github.com/ 2>&1 | tail -6
    echo "  rc=$? size=$(wc -c </tmp/gh.json 2>/dev/null)"
    echo
    echo "--- GNU wget https with explicit CA:"
    /usr/local/bin/wget --timeout=25 --tries=1 --ca-certificate=/usr/local/etc/ssl/certs/ca-certificates.crt \
        -O /tmp/gh2.json https://api.github.com/ 2>&1 | tail -4
    echo "  size=$(wc -c </tmp/gh2.json 2>/dev/null)"
    echo
    echo "--- real asset download (2MB appimagetool):"
    /usr/local/bin/wget --timeout=60 --tries=1 -O /tmp/at.AppImage \
      https://github.com/AppImage/AppImageKit/releases/download/13/obsolete-appimagetool-i686.AppImage 2>&1 | tail -4
    echo "  size=$(wc -c </tmp/at.AppImage 2>/dev/null)"
    echo "  head=$(head -c 4 /tmp/at.AppImage 2>/dev/null | od -c | head -1)"
    echo
    echo "--- xxri-store online: $(xxri-store online && echo YES || echo NO)"
    echo "=== WGET DONE ==="
  } > "$LOG" 2>&1
  sync
) >/dev/null 2>&1 &
