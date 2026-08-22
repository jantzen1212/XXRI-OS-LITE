# regress-hook.sh - Phase 10 regression probe (QA builds only).
# Exercises the subsystems the UI work could plausibly have broken and reports
# over the serial port: window manager, dock, Control Center, Store install
# (registry + .desktop + dock triplet + flwm menu + icon), and persistence.
(
  exec >/dev/ttyS0 2>&1
  echo "=== regress start"
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0\.0\.0\.0' && break; sleep 2; i=$((i+2)); done
  echo "dhcp ${i}s  XSERVER=$XSERVER"
  echo "WM        : $(pidof flwm >/dev/null && echo running || echo MISSING) ($(readlink -f /usr/local/bin/flwm))"
  echo "DOCK      : $(pidof wbar >/dev/null && echo running || echo MISSING)"
  echo "CC        : $(pgrep -f xxri-control-center >/dev/null && echo running || echo MISSING)"
  echo "XSERVER   : $(DISPLAY=:0 xdpyinfo 2>/dev/null | sed -n 's/.*dimensions: *\([0-9x]*\).*/\1/p' | head -1)"
  echo "XINPUT    : $(DISPLAY=:0 xdpyinfo 2>/dev/null | grep -c XInputExtension)"
  echo "FONTPATH  : $(DISPLAY=:0 xset q 2>/dev/null | grep -A1 'Font Path' | tail -1 | tr -d ' ' | cut -c1-60)"
  echo "APPS      : $(ls /usr/local/share/applications/*.desktop 2>/dev/null | wc -l) entries, tinycore-named: $(ls /usr/local/share/applications/ 2>/dev/null | grep -c '^tinycore-')"

  APP="${XXRI_TEST_APP:-leafpad}"
  echo "=== store install $APP"
  xxri-store refresh --force >/dev/null 2>&1
  t0=$(date +%s)
  xxri-store install "$APP" 2>&1 | tail -3
  n=0
  while [ $n -lt 60 ]; do
    [ -f "/usr/local/share/xxri-apps/$APP.reg" ] && break
    sleep 5; n=$((n+1))
  done
  echo "install took $(( $(date +%s) - t0 ))s"
  echo "REGISTRY  : $([ -f /usr/local/share/xxri-apps/$APP.reg ] && echo yes || echo NO)"
  echo "INTEGRATED: $(sed -n 's/^INTEGRATED=//p' /usr/local/share/xxri-apps/$APP.reg 2>/dev/null)"
  echo "DESKTOP   : $([ -f /usr/local/share/applications/xxri-app-$APP.desktop ] && echo yes || echo NO)"
  echo "DOCKENTRY : $(grep -c "xxri-app launch $APP" /usr/local/tce.icons 2>/dev/null)"
  echo "MENUITEM  : $(ls $HOME/.wmx/Applications/ 2>/dev/null | tr '\n' ' ')"
  echo "ICON      : $(wc -c < /usr/local/share/pixmaps/xxri-app-$APP.png 2>/dev/null) bytes"
  echo "VERIFY    : $(xxri-app verify-icons 2>&1 | tail -2 | tr '\n' ' ')"
  echo "DOCKPROC  : $(pidof wbar >/dev/null && echo running || echo MISSING)"
  sleep 3
  echo "GEOM      : $(DISPLAY=:0 xwininfo -root -tree 2>/dev/null | awk '/wbar/{for(i=1;i<=NF;i++) if ($i ~ /^[0-9]+x[0-9]+\+/) print $i}' | head -1)"
  echo "SHOTREADY installed"
  sleep 20
  echo "SHOTS_DONE"
  sleep 15
) &
