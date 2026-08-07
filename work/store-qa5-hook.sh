# Phase 9.1 multi-app QA hook (injected into /etc/skel/.X.d).
# Installs FIVE+ distinct real i686 AppImages through the Store's real backend,
# verifying each: install -> registry -> dock entry -> menu entry -> launch,
# then removes them all and checks nothing is orphaned.  Logs to ~/store-qa5.log.
(
  LOG="$HOME/store-qa5.log"
  DOCK=/usr/local/tce.icons          # the live dock icon list (wbar reads this)
  MENU="$HOME/.wmx/Applications"     # flwm Applications menu dir
  echo "=== Phase 9.1 multi-app QA  $(date) ===" > "$LOG"
  echo "arch: $(xxri-store arch)" >> "$LOG"

  # the five bundled real i686 apps (source=local, installed via the full
  # download->verify->integrate path) + registry/dock/menu/launch checks
  APPS="xxri-clock xxri-sysinfo xxri-calc xxri-notes xxri-hello"
  ok=0; n=0
  for id in $APPS; do
    n=$((n+1))
    echo "" >> "$LOG"
    echo "----- [$n] install $id -----" >> "$LOG"
    xxri-store install "$id" >> "$LOG" 2>&1
    # the registry id equals the catalog id (Store passes --as)
    reg="$(xxri-app info "$id" 2>/dev/null)"
    ver="$(echo "$reg" | sed -n 's/^VERSION=//p')"
    name="$(echo "$reg" | sed -n 's/^NAME=//p')"
    dsk="$(echo "$reg" | sed -n 's/^DESKTOP=//p')"
    icon="$(echo "$reg" | sed -n 's/^ICON=//p')"
    echo "  registry: name='$name' version=$ver" >> "$LOG"
    echo "  desktop file: $([ -f "$dsk" ] && echo yes || echo NO) ($dsk)" >> "$LOG"
    echo "  icon file:    $([ -s "$icon" ] && echo "yes $(wc -c <"$icon")B" || echo NO) ($icon)" >> "$LOG"
    echo "  dock entry:   $(grep -c "exec xxri-app launch $id" "$DOCK" 2>/dev/null) in tce.icons" >> "$LOG"
    echo "  menu entry:   $({ mn=$(echo "$name" | tr -d " "); [ -f "$MENU/$mn" ] && echo "yes ($mn)" || echo NO; })" >> "$LOG"
    # launch it and confirm the process comes up
    xxri-app launch "$id" >> "$LOG" 2>&1
    sleep 4
    if pgrep -f "$name" >/dev/null 2>&1; then echo "  launch: RUNNING" >> "$LOG"; ok=$((ok+1))
    else echo "  launch: not detected" >> "$LOG"; fi
    # close the launched terminal so the desktop screenshot later is clean
    pkill -f "$name" 2>/dev/null
  done
  echo "" >> "$LOG"
  echo "installed now ($(xxri-app list | grep -c '|') apps):" >> "$LOG"
  xxri-app list >> "$LOG" 2>&1

  # remove them all and prove no orphans (reg, desktop, icon, dock, menu)
  echo "" >> "$LOG"; echo "----- remove all -----" >> "$LOG"
  for id in $APPS; do
    name="$(xxri-app info "$id" 2>/dev/null | sed -n 's/^NAME=//p')"
    xxri-app remove "$id" --purge >> "$LOG" 2>&1
    orph=""
    [ -f "/usr/local/share/applications/xxri-app-$id.desktop" ] && orph="$orph desktop"
    [ -f "/usr/local/share/pixmaps/xxri-app-$id.png" ] && orph="$orph icon"
    grep -q "xxri-app-$id" "$DOCK" 2>/dev/null && orph="$orph dock"
    { mn=$(echo "$name" | tr -d " "); [ -n "$mn" ] && [ -f "$MENU/$mn" ] && orph="$orph menu"; }
    [ -f "/usr/local/share/xxri-apps/$id.reg" ] && orph="$orph reg"
    echo "  $id removed; orphans:${orph:- none}" >> "$LOG"
  done
  echo "" >> "$LOG"
  echo "installed after removal ($(xxri-app list | grep -c '|') apps):" >> "$LOG"
  xxri-app list >> "$LOG" 2>&1
  echo "" >> "$LOG"
  echo "RESULT: $ok/$n launched, $(xxri-app list | grep -c '|') left installed" >> "$LOG"
  echo "=== QA5 DONE ===" >> "$LOG"
  sync
) >/dev/null 2>&1 &
