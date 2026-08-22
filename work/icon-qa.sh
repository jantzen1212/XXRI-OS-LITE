#!/bin/bash
# icon-qa.sh PAGE [WAIT] [clean]  - boot the installed image (NON-snapshot so
# the icon cache persists between runs), open the Store on PAGE, screendump at
# WAIT seconds, then power down and pull ~/store-qa.log out with debugfs.
set -u
B=/home/jantzen/xxri-build; cd "$B"
D="$B/work/iconqa"; mkdir -p "$D/shots"
PAGE="${1:-home}"; WAIT="${2:-110}"; CLEAN="${3:-}"
SAFE="${PAGE//:/-}"
MON="$D/mon.sock"; LOG="$D/qemu.log"
ps -eo pid,args | grep 'iconqa/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
sleep 2; rm -f "$MON" "$D/shot.ppm"
APPEND="loglevel=3 storepage=$PAGE"
[ -n "${5:-}" ] && APPEND="$APPEND storeaudit=1"
[ "$CLEAN" = clean ] && APPEND="$APPEND storeclean=1"
setsid qemu-system-x86_64 -enable-kvm -m 2048 \
  -kernel work/iso/boot/vmlinuz -initrd output/core.gz \
  -append "$APPEND" \
  -drive file=output/xxri-disk.img,format=raw,if=ide \
  -vga std -display none -serial file:"$D/serial.log" \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -monitor unix:"$MON",server,nowait >"$LOG" 2>&1 &
for i in $(seq 1 20); do [ -S "$MON" ] && break; sleep 1; done
[ -S "$MON" ] || { echo "QEMU did not start"; tail -5 "$LOG"; exit 1; }
python3 -c "import time;time.sleep($WAIT)"
echo "screendump $D/shot.ppm" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1
python3 -c "import time;time.sleep(3)"
OUT="$D/shots/$SAFE.png"
magick "$D/shot.ppm" "$OUT" 2>/dev/null && echo "SHOT $OUT $(identify -format '%wx%h' "$OUT")"
# second shot a bit later catches late-landing icons
python3 -c "import time;time.sleep(${4:-45})"
echo "screendump $D/shot2.ppm" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1
python3 -c "import time;time.sleep(3)"
OUT2="$D/shots/$SAFE-late.png"
magick "$D/shot2.ppm" "$OUT2" 2>/dev/null && echo "SHOT $OUT2 $(identify -format '%wx%h' "$OUT2")"
echo "system_powerdown" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1
for i in $(seq 1 40); do ps -eo pid,args | grep -q 'iconqa/mon[.]sock' || break; sleep 2; done
ps -eo pid,args | grep 'iconqa/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
sleep 2
e2fsck -fy output/xxri-disk.img >/dev/null 2>&1
debugfs -R "dump /home/xxri/store-qa.log $D/store-qa.log" output/xxri-disk.img >/dev/null 2>&1
echo "--- store-qa.log ---"; tail -80 "$D/store-qa.log" 2>/dev/null
