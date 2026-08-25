#!/bin/bash
# vm-csd.sh - the window management that moved from flwm into the application.
# With no titlebar, Settings owns drag, minimise, maximise and close, so each
# has to be proven again with real pointer input.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
wl() { tail -30 work/vm/serial.log | grep '^WL ' | tail -1 | sed 's/^WL //'; }
px() { magick "work/vm/shots/$1.png" -crop 1x1+$2+$3 +repage txt: | tail -1 | grep -o '#[0-9A-F]*'; }

echo "== state now:            $(wl)"
echo "== DRAG from the sidebar header (150,20) -> (420,240)"
i drag 150 20 420 240; sleep 2; ./work/vm-shot.sh csd-drag >/dev/null 2>&1
echo "   pixel at old top-left (20,20) = $(px csd-drag 20 20)  (wallpaper means it moved)"
echo "   pixel at new top-left (300,240) = $(px csd-drag 300 240)"
echo "== MAXIMISE (controls moved with the window: now at +270,+220)"
i click 292 231; sleep 2; ./work/vm-shot.sh csd-max >/dev/null 2>&1
echo "   pixel at (10,10) = $(px csd-max 10 10)   (sidebar means it filled the screen)"
echo "== RESTORE"
i click 22 11; sleep 2; ./work/vm-shot.sh csd-restore >/dev/null 2>&1
echo "   pixel at (10,10) = $(px csd-restore 10 10)"
echo "== MINIMISE"
i click 7 11; sleep 3; ./work/vm-shot.sh csd-min >/dev/null 2>&1
echo "   windows: $(wl)"
echo "== RESTORE FROM DOCK"
i move 512 250; sleep 1; i click 441 719; sleep 6
echo "   windows: $(wl)"
echo "== CLOSE"
i click 7 11 >/dev/null 2>&1 || true
sleep 1
i click 37 11; sleep 3
echo "   windows: $(wl)"
exit 0
