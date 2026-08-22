#!/bin/bash
# icon-shots.sh PAGE[,PAGE...] [clean] [audit]  - one boot, many Store pages.
# Follows the guest's SHOTREADY markers on the serial line, so each screendump
# lands on the page it names no matter how the boot timing drifts.
set -u
B=/home/jantzen/xxri-build; cd "$B"
D="$B/work/iconqa"; mkdir -p "$D/shots"
PAGES="${1:-home}"; CLEAN="${2:-}"; AUDIT="${3:-}"
MON="$D/mon.sock"; SER="$D/serial.log"; LOG="$D/qemu.log"
ps -eo pid,args | grep 'iconqa/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
sleep 2; rm -f "$MON" "$SER"
APPEND="loglevel=3 storepages=$PAGES"
[ "$CLEAN" = clean ] && APPEND="$APPEND storeclean=1"
[ "$AUDIT" = audit ] && APPEND="$APPEND storeaudit=1"
setsid qemu-system-x86_64 -enable-kvm -m 2048 \
  -kernel work/iso/boot/vmlinuz -initrd output/core.gz -append "$APPEND" \
  -drive file=output/xxri-disk.img,format=raw,if=ide \
  -vga std -display none -serial file:"$SER" \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -monitor unix:"$MON",server,nowait >"$LOG" 2>&1 &
for i in $(seq 1 20); do [ -S "$MON" ] && break; sleep 1; done
[ -S "$MON" ] || { echo "QEMU did not start"; exit 1; }
seen=0
for i in $(seq 1 300); do
  python3 -c "import time;time.sleep(4)"
  [ -f "$SER" ] || continue
  n=$(grep -c '^SHOTREADY' "$SER" 2>/dev/null | head -1); n=${n:-0}
  while [ "$seen" -lt "$n" ]; do
    seen=$((seen+1))
    pg=$(grep '^SHOTREADY' "$SER" | sed -n "${seen}p" | awk '{print $2}' | tr -d '\r')
    safe="${pg//:/-}"
    rm -f "$D/s.ppm"
    echo "screendump $D/s.ppm" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1
    python3 -c "import time;time.sleep(3)"
    magick "$D/s.ppm" "$D/shots/$safe.png" 2>/dev/null && echo "SHOT $safe $(identify -format '%wx%h' "$D/shots/$safe.png")"
  done
  grep -q '^SHOTS_DONE' "$SER" 2>/dev/null && break
done
echo "system_powerdown" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1
for i in $(seq 1 30); do ps -eo pid,args | grep -q 'iconqa/mon[.]sock' || break; sleep 2; done
ps -eo pid,args | grep 'iconqa/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
sleep 2; e2fsck -fy output/xxri-disk.img >/dev/null 2>&1
grep -E '^(===|catalog=|dhcp)' "$SER" | tail -5
