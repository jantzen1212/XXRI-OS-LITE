# ui-qa-hook.sh - Phase 10 visual QA (QA builds only).
# Opens the windows named by the `uiscene=` boot code and announces
# "SHOTREADY <scene>" on the serial port once each is drawn, so the host can
# screendump the exact moment.  Scenes:
#   desktop     bare desktop + dock
#   settings    Settings on a page (uipage=<id>)
#   store       Store on a page
#   two         Settings + Store, to show active vs inactive titlebars
#   dialog      an xxri-dialog modal over Settings
#   maximize    Settings, then its maximize control clicked
SCENES=$(sed -n 's/.*\buiscene=\([a-zA-Z0-9:_.,-]*\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
[ -n "$SCENES" ] || SCENES=desktop
(
  exec >/dev/ttyS0 2>&1
  echo "=== ui-qa scenes=$SCENES"
  i=0; while [ $i -lt 40 ]; do route -n 2>/dev/null | grep -q '^0\.0\.0\.0' && break; sleep 2; i=$((i+2)); done
  echo "dhcp ${i}s; XSERVER=$XSERVER; res=$(xdpyinfo 2>/dev/null | sed -n 's/.*dimensions: *\([0-9x]*\).*/\1/p' | head -1)"
  echo "wm: $(pidof flwm >/dev/null && echo flwm-running || echo NO-WM)  dock: $(pidof wbar >/dev/null && echo wbar-running || echo NO-DOCK)"
  echo "MODES: $(DISPLAY=:0 xrandr 2>/dev/null | awk '/^ +[0-9]+x[0-9]+/{printf "%s ", $1}')"
  echo "OUTPUT: $(DISPLAY=:0 xrandr 2>/dev/null | awk '/ connected/{print $1, $3}')"
  for sc in $(echo "$SCENES" | tr ',' ' '); do
    pkill -x xxri-settings 2>/dev/null; pkill -x xxri-store-gui 2>/dev/null
    pkill -x xxri-dialog 2>/dev/null; pkill -x xxri-power-menu 2>/dev/null
    pkill -x aterm 2>/dev/null; pkill -x editor 2>/dev/null; pkill -x mnttool 2>/dev/null
    # restart the control center so every scene starts with it collapsed
    # `pkill -x xxri-control-center` never matches: Linux truncates comm to 15
    # characters.  Use the panel's own CLI instead.
    if [ "$sc" != cc ]; then DISPLAY=:0 timeout 5 xxri-control-center --close; sleep 1; fi
    sleep 1
    case "$sc" in
      desktop)  : ;;
      settings*) DISPLAY=:0 xxri-settings "${sc#settings:}" & ;;
      store*)    DISPLAY=:0 xxri-store-gui "${sc#store:}" & ;;
      two)       DISPLAY=:0 xxri-settings about & sleep 6; DISPLAY=:0 xxri-store-gui home & ;;
      dialog)    DISPLAY=:0 xxri-settings about & sleep 6
                 DISPLAY=:0 xxri-dialog ask "Remove application" "Remove Geany from this computer?" "Remove" "Cancel" & ;;
      cc)        # give the panel something to show: a real mixer level
                 xxri-audio volume set 65 >/dev/null 2>&1
                 echo "audio: $(xxri-audio volume --json 2>/dev/null | head -c 120)"
                 DISPLAY=:0 xxri-control-center --open & ;;
      power)     DISPLAY=:0 xxri-power-menu & ;;
      tools)     DISPLAY=:0 aterm & sleep 4; DISPLAY=:0 editor & sleep 4; DISPLAY=:0 mnttool & ;;
    esac
    sleep 26
    # Objective geometry, not eyeballing: every mapped top-level with its
    # name and rectangle, so centering / size regressions show up as numbers.
    echo "GEOM $sc"
    DISPLAY=:0 xwininfo -root -tree 2>/dev/null | \
      awk '/^ +0x/ && /[0-9]+x[0-9]+\+/ {
             g=""; for(i=1;i<=NF;i++) if ($i ~ /^[0-9]+x[0-9]+\+/) g=$i;
             nm=""; for(i=2;i<=NF;i++){ if ($i ~ /^\(/) break; nm=nm" "$i }
             if (g!="") printf "  %-46s %s\n", substr(nm,2,46), g }' | head -12
    echo "SHOTREADY $sc"
    sleep 20
  done
  echo "SHOTS_DONE"
  sleep 20
) &
