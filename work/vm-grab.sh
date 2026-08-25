#!/bin/bash
# vm-grab.sh - find the exact action after which the desktop stops accepting
# clicks.  Probe = click the Settings dock icon and see whether a window
# appears; the probe is run before and after each candidate action.
set -u
B=/home/jantzen/xxri-build; cd "$B"
Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
# a probe launches Settings from the dock; "alive" = the screen changed at a
# point the wallpaper occupies but a Settings window would cover
probe() {
  local tag="$1"
  i click 440 719; sleep 13
  ./work/vm-shot.sh "grab-$tag" >/dev/null 2>&1
  local px
  px=$(magick "work/vm/shots/grab-$tag.png" -crop 1x1+600+300 txt: | tail -1 | grep -o '#[0-9A-F]\{6\}')
  # wallpaper at 600,300 is a pale pink/blue; a Settings window there is #FFFFFF
  if [ "$px" = "#FFFFFF" ] || [ "$px" = "#EEECF7" ]; then echo "  probe $tag: CLICK WORKED (settings up, $px)";
  else echo "  probe $tag: NO RESPONSE ($px)"; fi
}
close_all() {   # close via the titlebar X so the next probe starts clean
  i click 47 13; sleep 3
}

echo "== probe 1: fresh desktop, no menu has ever been opened"
probe 1-fresh
close_all
echo "== action: open the desktop menu, dismiss with ESC"
i click 1005 300; sleep 2; ./work/vm-shot.sh grab-menu-open >/dev/null 2>&1
i key esc; sleep 2
probe 2-after-esc
close_all
echo "== action: open the desktop menu, dismiss by clicking the desktop again"
i click 1005 300; sleep 2
i click 700 500; sleep 2
probe 3-after-click-away
echo "== done"
exit 0
