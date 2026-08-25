#!/bin/bash
# vm-fix-test.sh - the interaction list for the Phase 10.5 fix.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
wl() { tail -30 work/vm/serial.log | grep '^WL ' | tail -1 | sed 's/^WL //'; }
px() { ./work/vm-shot.sh "$1" >/dev/null 2>&1; magick "work/vm/shots/$1.png" -crop 1x1+$2+$3 +repage txt: | tail -1 | grep -o '#[0-9A-F]*'; }
W="${1:-1024}"; H="${2:-768}"
DX=$(( (W-315)/2 + 40 + 47 )); DY=$((H-49))
echo "open           : $(wl)"
echo "maximise (22,11)"; i click 22 11; sleep 2
echo "  pixel near dock line = $(px f-max 500 $((H-90)))"
echo "restore  (22,11)"; i click 22 11; sleep 2
echo "  same pixel = $(px f-res 500 $((H-90)))"
echo "minimise (7,11)";  i click 7 11; sleep 3; echo "  $(wl)"
echo "restore from dock"; i move $((W/2)) 250; sleep 1; i click $DX $DY; sleep 7; echo "  $(wl)"
echo "drag header (150,20) -> (400,220)"; i drag 150 20 400 220; sleep 2
echo "  old corner (20,20) = $(px f-drag 20 20)"
echo "close (moved: 287,231)"; i click 287 231; sleep 3; echo "  $(wl)"
echo "reopen from dock"; i move $((W/2)) 250; sleep 1; i click $DX $DY; sleep 16; echo "  $(wl)"
