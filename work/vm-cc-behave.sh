#!/bin/bash
# vm-cc-behave.sh - the three states the Control Center must get right:
# opens on the corner, STAYS open while the pointer is on it, retracts when the
# pointer goes elsewhere.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
open_px() { magick "work/vm/shots/$1.png" -crop 1x1+800+620 +repage txt: | tail -1 | grep -o '#[0-9A-F]*'; }
i move 480 300; sleep 3
i move 1015 762; sleep 3; ./work/vm-shot.sh cb-1-corner >/dev/null 2>&1
i move 880 640; sleep 4; ./work/vm-shot.sh cb-2-on-panel >/dev/null 2>&1
i move 880 640; sleep 4; ./work/vm-shot.sh cb-3-still-on >/dev/null 2>&1
i move 300 200; sleep 4; ./work/vm-shot.sh cb-4-away >/dev/null 2>&1
for s in cb-1-corner cb-2-on-panel cb-3-still-on cb-4-away; do
  printf "  %-16s px(800,620)=%s\n" "$s" "$(open_px $s)"
done
echo "(panel body is light lavender; wallpaper there is strong blue)"
