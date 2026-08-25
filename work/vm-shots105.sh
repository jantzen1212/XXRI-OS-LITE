#!/bin/bash
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
s() { ./work/vm-shot.sh "$1" >/dev/null 2>&1; echo "  $1"; }
i move 512 250; sleep 2; s p105-01-desktop
i move 1015 762; sleep 4; s p105-02-control-center
i move 512 250; sleep 3
i click 441 719; sleep 16; i move 700 450; sleep 1; s p105-03-settings-integrated
i click 392 719; sleep 20; i move 700 450; sleep 1; s p105-04-store-integrated
i click 536 719; sleep 8; i move 700 450; sleep 1; s p105-05-three-apps
i drag 500 11 300 300; sleep 2; s p105-06-dragged-window
i move 1015 762; sleep 4; s p105-07-transparency-over-app
i move 512 250; sleep 3; s p105-08-dock-indicators
