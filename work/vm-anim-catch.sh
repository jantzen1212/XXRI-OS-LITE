#!/bin/bash
# vm-anim-catch.sh - catch the minimise flight in the act.  Run against an image
# carrying a deliberately slowed window manager, so a screendump (which takes a
# couple of seconds) can land in the middle of the animation.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
./work/vm-input.py "$Q" move 512 250 >/dev/null
./work/vm-input.py "$Q" click 441 719 >/dev/null      # launch Settings (flies OUT)
sleep 1
./work/vm-shot.sh anim-launch-midflight >/dev/null 2>&1
sleep 14
./work/vm-shot.sh anim-settled >/dev/null 2>&1
# now minimise it: the frame should travel down toward the dock icon
( ./work/vm-input.py "$Q" click 18 13 >/dev/null & ) 
sleep 1
./work/vm-shot.sh anim-minimise-midflight >/dev/null 2>&1
sleep 4
./work/vm-shot.sh anim-minimised >/dev/null 2>&1
echo done
