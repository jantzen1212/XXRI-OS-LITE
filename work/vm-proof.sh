#!/bin/bash
# vm-proof.sh - put the Store's translucent rail over the DARK terminal and
# compare the same pixels with the rail over wallpaper.  A painted-on
# background cannot change; a composited one must.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
i move 512 250; sleep 2
i click 392 719; sleep 22; i move 900 600; sleep 2      # Store, over wallpaper
./work/vm-shot.sh P1-store-over-wallpaper >/dev/null 2>&1; echo "  P1 (rail over wallpaper)"
i click 536 719; sleep 9                                 # Terminal on top
# drag the terminal so it sits under where the Store's rail is
i drag 300 13 120 300; sleep 2
i click 392 719; sleep 6                                 # raise the Store again
i move 900 600; sleep 2
./work/vm-shot.sh P2-store-over-terminal >/dev/null 2>&1; echo "  P2 (rail over terminal)"
