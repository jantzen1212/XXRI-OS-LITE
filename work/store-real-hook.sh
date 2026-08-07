# Phase 9.2 Part 6: install a REAL app from the internet, end to end.
( LOG="$HOME/store-real.log"
  DOCK=/usr/local/tce.icons; MENU="$HOME/.wmx/Applications"
  # wait for DHCP before testing anything network-related
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  echo "=== production install test $(date) (net after ${i}s) ===" > "$LOG"
  echo "arch: $(xxri-store arch)" >> "$LOG"
  echo "network: $(xxri-store online && echo online || echo offline)" >> "$LOG"
  echo "repo(before refresh): $(xxri-store repo-path)" >> "$LOG"
  echo "--- refresh from the online repository ---" >> "$LOG"
  xxri-store refresh --force >> "$LOG" 2>&1
  echo "repo(after): $(xxri-store repo-path)" >> "$LOG"
  ID=appimagetool
  echo "" >> "$LOG"; echo "--- resolve $ID ---" >> "$LOG"
  xxri-store resolve $ID >> "$LOG" 2>&1
  echo "--- install $ID (real HTTPS download from GitHub) ---" >> "$LOG"
  xxri-store install $ID >> "$LOG" 2>&1
  reg="$(xxri-app info $ID 2>/dev/null)"
  nm="$(echo "$reg" | sed -n 's/^NAME=//p')"
  echo "  registry NAME=$nm VERSION=$(echo "$reg" | sed -n 's/^VERSION=//p')" >> "$LOG"
  echo "  file: $(echo "$reg" | sed -n 's/^FILE=//p')" >> "$LOG"
  echo "  desktop: $([ -f "$(echo "$reg"|sed -n 's/^DESKTOP=//p')" ] && echo yes || echo NO)" >> "$LOG"
  echo "  icon:    $([ -s "$(echo "$reg"|sed -n 's/^ICON=//p')" ] && echo yes || echo NO)" >> "$LOG"
  echo "  dock:    $(grep -c "exec xxri-app launch $ID" $DOCK 2>/dev/null)" >> "$LOG"
  echo "  menu:    $({ m=$(echo "$nm"|tr -d ' '); [ -f "$MENU/$m" ] && echo yes || echo NO; })" >> "$LOG"
  echo "--- launch ---" >> "$LOG"
  xxri-app launch $ID >> "$LOG" 2>&1
  sleep 5
  pgrep -f "$nm" >/dev/null 2>&1 && echo "  launch: RUNNING" >> "$LOG" || echo "  launch: (cli tool, exits immediately)" >> "$LOG"
  echo "--- installed list ---" >> "$LOG"; xxri-app list >> "$LOG" 2>&1
  echo "--- remove ---" >> "$LOG"
  xxri-app remove $ID --purge >> "$LOG" 2>&1
  orph=""
  [ -f "/usr/local/share/applications/xxri-app-$ID.desktop" ] && orph="$orph desktop"
  grep -q "exec xxri-app launch $ID" $DOCK 2>/dev/null && orph="$orph dock"
  [ -f "/usr/local/share/xxri-apps/$ID.reg" ] && orph="$orph reg"
  echo "  orphans after remove:${orph:- none}" >> "$LOG"
  echo "=== REAL DONE ===" >> "$LOG"; sync
) >/dev/null 2>&1 &
