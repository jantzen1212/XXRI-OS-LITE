#!/bin/bash
# vm-stack-test.sh - the four compositions, in both stacking orders.  Reversing
# the order is the test that separates real compositing from a painted-on
# background: a fake background cannot change when a different window moves
# behind it.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
i move 512 250; sleep 2
i click 392 719; sleep 22; i move 900 600; sleep 2
./work/vm-shot.sh S1-store-alone >/dev/null 2>&1; echo "  S1-store-alone"
i click 441 719; sleep 18; i move 900 600; sleep 2
./work/vm-shot.sh S2-settings-over-store >/dev/null 2>&1; echo "  S2-settings-over-store"
# raise the Store again by clicking its dock icon (focus, not relaunch)
i click 392 719; sleep 6; i move 900 600; sleep 2
./work/vm-shot.sh S3-store-over-settings >/dev/null 2>&1; echo "  S3-store-over-settings"
