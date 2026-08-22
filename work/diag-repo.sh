( LOG="$HOME/diag.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  hr(){ say ""; say "───────── $* ─────────"; }
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done

  say "=== REPOSITORY DIAGNOSIS $(date) ==="
  H=repo.xxri.flows.best
  U=https://$H/i686.json

  hr "1. DNS"
  say "  /etc/resolv.conf: $(tr '\n' ' ' < /etc/resolv.conf 2>/dev/null)"
  say "  nslookup:"
  nslookup $H 2>&1 | head -8 | sed 's/^/    /' >> "$LOG"

  hr "2. HTTPS connectivity"
  say "  GNU wget : $(command -v /usr/local/bin/wget || echo MISSING)"
  say "  busybox wget: $(busybox wget --help 2>&1 | head -1)"
  say "  curl     : $(command -v curl || echo MISSING)"
  say "  openssl  : $(command -v openssl || echo MISSING)"

  hr "3. TLS / CA bundle"
  for c in /usr/local/etc/ssl/certs/ca-certificates.crt /usr/local/etc/ssl/cacert.pem; do
    say "  $c : $([ -s "$c" ] && echo "$(wc -c < $c) bytes, $(grep -c 'BEGIN CERT' $c) certs" || echo MISSING)"
  done
  say "  handshake:"
  echo | timeout 20 openssl s_client -connect $H:443 -servername $H 2>&1 | \
     grep -iE 'CONNECTED|subject=|issuer=|Verify return code|Protocol|Cipher' | head -8 | sed 's/^/    /' >> "$LOG"

  hr "4. HTTP response headers"
  /usr/local/bin/wget -S --spider --timeout=25 --tries=1 \
     --ca-certificate=/usr/local/etc/ssl/certs/ca-certificates.crt \
     -U xxri-store "$U" 2>&1 | head -20 | sed 's/^/    /' >> "$LOG"

  hr "5. actual download"
  rm -f /tmp/dl.json
  /usr/local/bin/wget -q --timeout=45 --tries=2 \
     --ca-certificate=/usr/local/etc/ssl/certs/ca-certificates.crt \
     -U xxri-store -O /tmp/dl.json "$U" 2>/tmp/dl.err
  say "  wget exit=$?  size=$(wc -c < /tmp/dl.json 2>/dev/null || echo 0)"
  head -3 /tmp/dl.err 2>/dev/null | sed 's/^/    err: /' >> "$LOG"

  hr "6. file contents"
  say "  first 12 lines:"; head -12 /tmp/dl.json 2>/dev/null | sed 's/^/    /' >> "$LOG"
  say "  last 8 lines:";  tail -8  /tmp/dl.json 2>/dev/null | sed 's/^/    /' >> "$LOG"

  hr "7. JSON sanity"
  say "  has schema_version : $(grep -c '\"schema_version\"' /tmp/dl.json)"
  say "  has applications   : $(grep -c '\"applications\"' /tmp/dl.json)"
  say "  \"id\" occurrences   : $(grep -o '\"id\"' /tmp/dl.json | wc -l)"

  hr "8/9. parser + application count"
  say "  backend refresh -> $(xxri-store refresh --force 2>&1)"
  say "  cached catalog  -> $(wc -c < $HOME/.cache/xxri-store/i686.json 2>/dev/null || echo NONE)"
  say "  status          -> $(xxri-store status)"
  say "  ids parsed      -> $(xxri-store field __count __count 2>/dev/null)"
  n=0
  for a in $(sed -n 's/.*"id"[ ]*:[ ]*"\([^"]*\)".*/\1/p' "$HOME/.cache/xxri-store/i686.json" 2>/dev/null); do n=$((n+1)); done
  say "  ids via sed     -> $n"

  hr "10. do applications reach the UI model?"
  say "  (GUI --dump prints exactly what the model holds)"
  for a in firefox gimp telegram-desktop; do
    say "  --- $a ---"
    xxri-store-gui --dump "$a" 2>/dev/null | grep -E "id +:|name +:|category +:|kind +:|verified|unavailable|app_visible|app_installable|icon_url" | sed 's/^/    /' >> "$LOG"
  done

  hr "11. category mapping"
  say "  raw categories in the catalog:"
  sed -n 's/.*"category"[ ]*:[ ]*"\([^"]*\)".*/\1/p' "$HOME/.cache/xxri-store/i686.json" 2>/dev/null | sort | uniq -c | sort -rn | head -12 | sed 's/^/    /' >> "$LOG"

  hr "12. rendering"
  echo "store" > "$HOME/stage"; sync
  xxri-store-gui >/tmp/gui.out 2>&1 &
  sleep 25
  say "  gui alive: $(ps -eo comm | grep -c xxri-store-gui)"
  say "  gui stderr:"; head -8 /tmp/gui.out | sed 's/^/    /' >> "$LOG"
  sleep 25
  echo DONE > "$HOME/stage"
  say "=== DIAG DONE ==="; sync
) >/dev/null 2>&1 &
