( LOG="$HOME/verify.log"; S="$HOME/stage"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done

  say "=== RUNTIME VERIFICATION under X.Org $(date) ==="
  say "  X server : ${XSERVER:-?}   $(xdpyinfo 2>/dev/null | awk '/dimensions/{print $2}')"
  say "  extensions: $(for e in XInputExtension XKEYBOARD RANDR MIT-SHM; do xdpyinfo 2>/dev/null | grep -q $e && printf '%s ' $e; done)"
  say "  WM        : $(ps -eo comm | grep -cE '^flwm$') flwm"
  say ""

  # window whose WM_CLASS matches $1 -> prints "MAPPED WxH" or "unmapped"/"none"
  # NOTE: must walk the FULL tree, not -children.  A window manager reparents a
  # window when it maps it, so a mapped client is a grandchild of the root and
  # never appears in `xwininfo -root -children` - scanning only root children
  # reports every working application as "unmapped".
  winstate() {
      best="none"
      for w in $(xwininfo -root -tree 2>/dev/null | awk '/0x/{print $1}'); do
          xprop -id "$w" WM_CLASS 2>/dev/null | grep -qi "$1" || continue
          st=$(xwininfo -id "$w" 2>/dev/null | grep 'Map State' | awk '{print $3}')
          wd=$(xwininfo -id "$w" 2>/dev/null | awk '/^  Width:/{print $2}')
          ht=$(xwininfo -id "$w" 2>/dev/null | awk '/^  Height:/{print $2}')
          [ "$st" = IsViewable ] && [ "${wd:-0}" -gt 200 ] && { echo "MAPPED ${wd}x${ht}"; return; }
          [ "$best" = none ] && best="unmapped"
      done
      echo "$best"
  }

  verify() { # id  wm_class  [launch-cmd]
      ID="$1"; CLS="$2"; CMD="$3"
      say ""; say "############ $ID ############"
      echo "$ID:install" > "$S"; sync
      t0=$(date +%s)
      R="$(xxri-store install "$ID" 2>&1 | tail -1)"
      say "  install        : $R  ($(( $(date +%s) - t0 ))s)"
      reg="$(xxri-app info "$ID" 2>/dev/null)"
      [ -z "$reg" ] && { say "  RESULT: INSTALL FAILED"; return; }
      NAME="$(echo "$reg" | sed -n 's/^NAME=//p')"
      B="$(echo "$reg" | sed -n 's/^FILE=//p')"
      ICON="$(echo "$reg" | sed -n 's/^ICON=//p')"
      say "  name           : $NAME"
      say "  binary         : $B"
      say "  installed page : $(xxri-store installed 2>/dev/null | grep -c "\"id\":\"$ID\"")"
      say "  icon file      : $([ -s "$ICON" ] && echo "yes $(wc -c < "$ICON")B" || echo NO)"
      say "  desktop entry  : $([ -f /usr/local/share/applications/xxri-app-$ID.desktop ] && echo yes || echo NO)"
      say "  dock entry     : $(grep -c "launch $ID\$" /usr/local/tce.icons 2>/dev/null)"
      say "  menu entry     : $([ -f "$HOME/.wmx/Applications/$(echo "$NAME"|tr -d ' ')" ] && echo yes || echo NO)"
      say "  ldd missing    : $(ldd "$B" 2>&1 | grep -c 'not found')"
      ldd "$B" 2>&1 | grep 'not found' | head -3 | sed 's/^/      /' >> "$LOG"

      echo "$ID:run" > "$S"; sync
      if [ -n "$CMD" ]; then ( cd "$HOME"; timeout 120 $CMD ) >/tmp/$ID.out 2>&1 &
      else ( cd "$HOME"; timeout 120 "$B" ) >/tmp/$ID.out 2>&1 & fi
      sleep 55
      W="$(winstate "$CLS")"
      say "  WINDOW         : $W"
      say "  process alive  : $(ps -eo args | grep -v grep | grep -c "$B")"
      say "  pixbuf errors  : $(grep -ci 'recognize the image file format' /tmp/$ID.out)"
      grep -iE 'error|cannot|fail' /tmp/$ID.out 2>/dev/null | grep -viE 'alsa|pulse|gnutls|XInput' | head -3 | sed 's/^/      /' >> "$LOG"
      case "$W" in MAPPED*) echo "$ID:SHOT" > "$S"; sync; sleep 30
                   say "  RESULT         : PASS - window visible, stayed up 30s+" ;;
                *) say "  RESULT         : window not visible ($W)" ;;
      esac
      for p in $(ps -eo pid,args | grep -v grep | grep "$B" | awk '{print $1}'); do kill $p 2>/dev/null; done
      sleep 6
      for p in $(ps -eo pid,args | grep -v grep | grep "$B" | awk '{print $1}'); do kill -9 $p 2>/dev/null; done
      sleep 3
  }

  for spec in $XXRI_VERIFY; do
      id="${spec%%:*}"; cls="${spec##*:}"
      verify "$id" "$cls"
  done
  echo DONE > "$S"
  say ""; say "=== VERIFY DONE ==="; sync
) >/dev/null 2>&1 &
