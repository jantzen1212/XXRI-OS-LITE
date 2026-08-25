#!/bin/bash
# vm-s1s2.sh - on-device acceptance for subsystems 1 and 2, driven with REAL
# pointer events (usb-tablet + QMP input-send-event).
#
# The pre-fix build died the first time the desktop menu was opened with a
# populated ~/.wmx, taking every window decoration with it.  This walks the
# same path many times over and shoots the result, so "flwm stayed alive" is a
# picture of a titlebar, not an assertion.
set -u
B=/home/jantzen/xxri-build; cd "$B"
Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
shot() { ./work/vm-shot.sh "$1" >/dev/null 2>&1; echo "   shot $1"; }
DOCK_Y=719; SETTINGS_X=440; TERM_X=536; STORE_X=392
DESK_X=1005; DESK_Y=300

echo "== launch Settings from the dock"
i click $SETTINGS_X $DOCK_Y; sleep 14; shot s1-01-settings

echo "== open + dismiss the desktop menu 6 times (the old crash path)"
for n in 1 2 3 4 5 6; do
  i click $DESK_X $DESK_Y; sleep 2
  [ $n = 1 ] && shot s1-02-menu-open
  i key esc; sleep 1
done
shot s1-03-after-6-menus

echo "== launch a second app, then menu again"
i click $TERM_X $DOCK_Y; sleep 8; shot s1-04-two-windows
i click $DESK_X $DESK_Y; sleep 2; shot s1-05-menu-two-clients
i key esc; sleep 1

echo "== titlebar controls on the focused window"
i click 700 400; sleep 1            # focus the terminal
shot s2-01-focused
i click 33 13;  sleep 2; shot s2-02-maximised
i click 33 13;  sleep 2; shot s2-03-restored
i click 18 13;  sleep 2; shot s2-04-minimised
echo "== restore the minimised window from the desktop menu"
i click $DESK_X $DESK_Y; sleep 2; shot s2-05-menu-with-iconic
i key esc; sleep 1

echo "== drag the titlebar"
i drag 300 13 480 220; sleep 2; shot s2-06-dragged

echo "== close"
i click 48 13; sleep 3; shot s2-07-closed
i click $DESK_X $DESK_Y; sleep 2; shot s2-08-menu-after-close
i key esc; sleep 1
echo "== done"
exit 0
