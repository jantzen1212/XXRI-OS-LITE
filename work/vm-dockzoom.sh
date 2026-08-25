#!/bin/bash
# vm-dockzoom.sh - is the dock's hover zoom what makes clicks miss?
# Same five launchers, but the pointer is parked away from the dock before each
# click so every icon is clicked at its RESTING position.
set -u
B=/home/jantzen/xxri-build; cd "$B"
Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
tree() { tail -40 work/vm/serial.log | grep '^DW ' | tail -1 | sed 's/.*win=//'; }
NAMES="Store Settings Disks Terminal Editor"
XS="392 440 488 536 583"
echo "=== pointer parked away before each click ==="
set -- $XS
for nm in $NAMES; do
  x=$1; shift
  i move 512 250          # park: dock returns to its resting geometry
  sleep 2
  i click $x 719
  sleep 12
  echo "  $nm (x=$x) -> $(tree)"
done
echo "=== done"
exit 0
