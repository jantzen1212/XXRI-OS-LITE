# icon-qa-hook.sh - QA only.  Walks the Store through the pages named by the
# `storepages=` boot code (comma separated), announcing "SHOTREADY <page>" on
# the serial port once each page has had time to draw, so the host can
# screendump exactly the right moment instead of guessing at a boot's timing.
#   storeclean=1   wipe the Store cache first (true cold start)
#   storeaudit=1   fetch every catalog icon and report, per application,
#                  whether the Store decoded a real image or fell back to a tile
PAGES=$(sed -n 's/.*\bstorepages=\([a-zA-Z0-9:_.,-]*\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
[ -n "$PAGES" ] || PAGES=home
CLEAN=$(sed -n 's/.*\bstoreclean=\([0-9]\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
AUDIT=$(sed -n 's/.*\bstoreaudit=\([0-9]\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
C=$HOME/.cache/xxri-store
(
  exec >/dev/ttyS0 2>&1
  echo "=== icon-qa pages=$PAGES clean=$CLEAN audit=$AUDIT"
  df -h / | tail -1
  [ "$CLEAN" = 1 ] && { rm -rf "$C"; echo "cache wiped"; }
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0\.0\.0\.0' && break; sleep 2; i=$((i+2)); done
  echo "dhcp ${i}s"
  xxri-store refresh --force >/dev/null 2>&1
  echo "catalog=$(wc -c < $C/i686.json 2>/dev/null)"
  if [ "$AUDIT" = 1 ]; then
    IDS=$(grep -o '"id": *"[^"]*"' "$C/i686.json" | sed 's/.*: *"//;s/"$//')
    xxri-store icons $IDS >/dev/null 2>&1
    real=0; tile=0; bad=""
    for id in $IDS; do
      r=$(/usr/local/bin/xxri-store-gui --dump "$id" 2>/dev/null | sed -n 's/.*from real file : \([01]\).*/\1/p' | head -1)
      b=$(wc -c < "$C/icons/$id.png" 2>/dev/null || echo 0)
      [ "$r" = 1 ] && real=$((real+1)) || { tile=$((tile+1)); bad="$bad $id"; }
      echo "ICON $id real=$r bytes=$b"
    done
    echo "=== AUDIT real=$real tile=$tile bad:$bad"
  fi
  for pg in $(echo "$PAGES" | tr ',' ' '); do
    pkill -x xxri-store-gui 2>/dev/null
    sleep 2
    XXRI_STORE_DEBUG=1 DISPLAY=:0 /usr/local/bin/xxri-store-gui "$pg" &
    sleep 30
    echo "SHOTREADY $pg"
    sleep 22
  done
  echo "SHOTS_DONE"
  sleep 30
) &
