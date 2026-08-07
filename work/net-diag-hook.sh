# Diagnose network + TLS on the device.
( LOG="$HOME/netdiag.log"
  echo "=== net/TLS diagnostic $(date) ===" > "$LOG"
  # wait up to 60s for a default route (DHCP may still be running)
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  echo "waited ${i}s for default route" >> "$LOG"
  echo "--- ifconfig:" >> "$LOG"; ifconfig 2>/dev/null | grep -E 'eth|inet ' >> "$LOG"
  echo "--- route:" >> "$LOG"; route -n 2>/dev/null | head -4 >> "$LOG"
  echo "--- resolv.conf:" >> "$LOG"; cat /etc/resolv.conf 2>/dev/null >> "$LOG"
  echo "--- wget HTTP  test:" >> "$LOG"
  wget -q -T 10 -O /tmp/h.txt http://1.1.1.1/ 2>>"$LOG" && echo "  HTTP OK ($(wc -c </tmp/h.txt) bytes)" >> "$LOG" || echo "  HTTP FAILED" >> "$LOG"
  echo "--- wget HTTPS test (github):" >> "$LOG"
  wget -T 15 -O /tmp/s.txt https://api.github.com/ 2>>"$LOG" && echo "  HTTPS OK ($(wc -c </tmp/s.txt) bytes)" >> "$LOG" || echo "  HTTPS FAILED" >> "$LOG"
  echo "--- ssl helpers present:" >> "$LOG"
  for f in /usr/bin/ssl_client /usr/local/bin/ssl_client /usr/local/bin/openssl /usr/bin/wget /usr/local/bin/wget; do
    [ -e "$f" ] && echo "  $f yes" >> "$LOG" || echo "  $f no" >> "$LOG"
  done
  echo "--- ca certs:" >> "$LOG"
  ls -d /usr/local/etc/ssl/certs /etc/ssl/certs /usr/local/share/ca-certificates 2>/dev/null >> "$LOG" || echo "  none" >> "$LOG"
  echo "--- openssl s_client probe:" >> "$LOG"
  echo | timeout 15 openssl s_client -connect github.com:443 -brief 2>&1 | head -5 >> "$LOG"
  echo "=== NET DONE ===" >> "$LOG"; sync
) >/dev/null 2>&1 &
