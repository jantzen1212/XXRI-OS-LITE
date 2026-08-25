#!/bin/bash
# vm-final-qa.sh RES - capture the Phase 10.2 acceptance scenes at one
# resolution, driving the guest with real pointer input throughout.
set -u
B=/home/jantzen/xxri-build; cd "$B"
RES="${1:-1024x768}"; W=${RES%x*}; H=${RES#*x}
Q=work/vm/qmp.sock
export XXRI_RES="$RES"
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
shot() { ./work/vm-shot.sh "$RES-$1" >/dev/null 2>&1; echo "   $RES-$1"; }
park() { i move $((W/2)) $((H/3)); sleep 2; }
# the dock pill is 315px wide and centred; icons are 40px on a 47px pitch
DX=$(( (W-315)/2 + 40 )); DY=$((H-49))
ic() { echo $((DX + $1*47)); }

echo "-- $RES: clean desktop"
park; shot desktop
echo "-- $RES: Control Center by bottom-right HOVER"
i move $((W-9)) $((H-6)); sleep 4; shot cc-hover
park; sleep 3
echo "-- $RES: Control Center by explicit chip CLICK"
i click $((W-70)) $((H-49)); sleep 3; shot cc-click
park; sleep 3
echo "-- $RES: Settings (titlebar + close button as X)"
i click $(ic 1) $DY; sleep 15; park; shot settings
echo "-- $RES: Store"
i click $(ic 0) $DY; sleep 18; park; shot store
echo "-- $RES: Terminal, three windows, dock indicators"
i click $(ic 3) $DY; sleep 9; park; shot three-windows
shot dock-indicators
echo "-- $RES: maximise / restore the focused window"
i click 33 13; sleep 2; shot maximised
i click 33 13; sleep 2; shot restored
echo "-- $RES: minimise, then restore FROM THE DOCK"
i click 18 13; sleep 2; park; shot minimised
i click $(ic 1) $DY; sleep 6; park; shot dock-restored
echo "-- $RES: desktop menu"
i click $((W-14)) $((H/2)); sleep 2; shot desktop-menu
i key esc; sleep 1
exit 0
