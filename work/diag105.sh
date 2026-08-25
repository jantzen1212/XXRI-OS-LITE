#!/bin/bash
# diag105.sh - reproduce the reported composition and measure it.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
i move 512 250; sleep 2
i click 392 719; sleep 22            # Store
i move 512 250; sleep 2
./work/vm-shot.sh D-store-alone >/dev/null 2>&1; echo "  D-store-alone"
i click 441 719; sleep 18            # Settings over Store
i move 900 600; sleep 2
./work/vm-shot.sh D-settings-over-store >/dev/null 2>&1; echo "  D-settings-over-store"
