# Phase 9.2 final verification: repository chain + both GUIs on screen.
( i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  LOG="$HOME/final.log"
  export REMOTE_BASE="http://10.0.2.2:8099/store"
  { echo "=== repository chain ==="
    echo "1. bundled (offline fallback) : $(xxri-store repo-path)"
    echo "2. online?                    : $(xxri-store online && echo yes || echo no)"
    echo "3. refresh from remote        : $(xxri-store refresh --force 2>&1 | tail -1)"
    echo "4. active repo now            : $(xxri-store repo-path)"
    echo "5. apps in cached repo        : $(wc -l < "$(xxri-store repo-path)/index.tsv")"
    echo "6. i686-installable+verified  : $(awk -F'\t' '$4 ~ /(^| )i686( |$)/ && $16==1' "$(xxri-store repo-path)/index.tsv" | wc -l)"
  } > "$LOG" 2>&1
  sync
  sh -c 'exec /usr/local/bin/xxri-settings' >/dev/null 2>&1 &
  sleep 8
  REMOTE_BASE="http://10.0.2.2:8099/store" sh -c 'exec /usr/local/bin/xxri-store-gui' >/dev/null 2>&1 &
) &
