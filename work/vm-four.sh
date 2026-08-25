#!/bin/bash
# vm-four.sh - the four compositions the brief asks for, in both stacking orders.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
i move 512 250; sleep 2
i click 441 719; sleep 18; i move 900 620; sleep 2
./work/vm-shot.sh F1-settings-alone >/dev/null 2>&1; echo "  F1-settings-alone"
i click 7 11; sleep 3                       # minimise Settings out of the way
i move 512 250; sleep 2
i click 392 719; sleep 24; i move 900 620; sleep 2
./work/vm-shot.sh F2-store-alone >/dev/null 2>&1; echo "  F2-store-alone"
i move 512 250; sleep 1
i click 441 719; sleep 8; i move 900 620; sleep 2    # restore Settings on top
./work/vm-shot.sh F3-settings-over-store >/dev/null 2>&1; echo "  F3-settings-over-store"
i move 512 250; sleep 1
i click 392 719; sleep 6; i move 900 620; sleep 2    # raise Store on top
./work/vm-shot.sh F4-store-over-settings >/dev/null 2>&1; echo "  F4-store-over-settings"
