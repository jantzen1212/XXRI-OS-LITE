#!/bin/bash
# ui-shots.sh SCENE[,SCENE...] [RES] - boot the installed image and screendump
# each Phase 10 UI scene when the guest says it is drawn.  IMG=<path> aims a
# run at a copy so the shipped image stays untouched.
set -u
B=/home/jantzen/xxri-build; cd "$B"
D="$B/work/uiqa"; mkdir -p "$D/shots"
SCENES="${1:-desktop}"; RES="${2:-1024x768}"
IMG="${IMG:-output/xxri-disk.img}"
MON="$D/mon.sock"; SER="$D/serial.log"; LOG="$D/qemu.log"
ps -eo pid,args | grep 'uiqa/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
sleep 2; rm -f "$MON" "$SER"
setsid qemu-system-x86_64 -enable-kvm -m 2048 \
  -kernel work/iso/boot/vmlinuz -initrd output/core.gz \
  -append "loglevel=3 xres=$RES uiscene=$SCENES ${REGRESS:+regress=1}" \
  -drive file="$IMG",format=raw,if=ide \
  -vga std -display none -serial file:"$SER" \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -audiodev none,id=snd0 -device AC97,audiodev=snd0 \
  -monitor unix:"$MON",server,nowait >"$LOG" 2>&1 &
for i in $(seq 1 20); do [ -S "$MON" ] && break; sleep 1; done
[ -S "$MON" ] || { echo "QEMU did not start"; exit 1; }
seen=0
for i in $(seq 1 400); do
  python3 -c "import time;time.sleep(4)"
  [ -f "$SER" ] || continue
  n=$(grep -c '^SHOTREADY' "$SER" 2>/dev/null | head -1); n=${n:-0}
  while [ "$seen" -lt "$n" ]; do
    seen=$((seen+1))
    sc=$(grep '^SHOTREADY' "$SER" | sed -n "${seen}p" | awk '{print $2}' | tr -d '\r')
    rm -f "$D/s.ppm"
    echo "screendump $D/s.ppm" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1
    python3 -c "import time;time.sleep(3)"
    magick "$D/s.ppm" "$D/shots/${RES}-${sc//:/-}.png" 2>/dev/null && \
      echo "SHOT ${RES}-${sc//:/-} $(identify -format '%wx%h' "$D/shots/${RES}-${sc//:/-}.png")"
  done
  grep -q '^SHOTS_DONE' "$SER" 2>/dev/null && break
  # HOLD=1 keeps the VM running after the scenes so ui-pointer.sh can drive it
done
if [ -n "${HOLD:-}" ]; then echo "VM HELD on $MON - run work/ui-pointer.sh, then kill it"; exit 0; fi
echo "system_powerdown" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1
for i in $(seq 1 30); do ps -eo pid,args | grep -q 'uiqa/mon[.]sock' || break; sleep 2; done
ps -eo pid,args | grep 'uiqa/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
sleep 2; e2fsck -fy "$IMG" >/dev/null 2>&1
grep -E '^(===|wm:|dhcp|windows:)' "$SER" | tail -8
