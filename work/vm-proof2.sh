#!/bin/bash
# vm-proof2.sh - the Terminal at its default position sits behind the Store's
# full-width translucent TOOLBAR.  Same toolbar pixels, wallpaper vs dark
# window behind: a painted background cannot change, a composited one must.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
i move 512 250; sleep 2
i click 392 719; sleep 22; i move 900 600; sleep 2
./work/vm-shot.sh Q1-toolbar-over-wallpaper >/dev/null 2>&1; echo "  Q1"
i click 536 719; sleep 9                    # Terminal, default placement
i click 392 719; sleep 6                    # raise the Store over it
i move 900 600; sleep 2
./work/vm-shot.sh Q2-toolbar-over-terminal >/dev/null 2>&1; echo "  Q2"
