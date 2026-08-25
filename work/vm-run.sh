#!/bin/bash
# vm-run.sh [RES] - boot the installed image and HOLD it, with a QMP socket and
# an absolute pointer, so work/vm-input.py can really click and drag.
#   IMG=<path>   boot a copy instead of the shipped image
#   ISO=1        boot the live ISO (with a blank disk attached), as a user would
set -u
B=/home/jantzen/xxri-build; cd "$B"
D="$B/work/vm"; mkdir -p "$D/shots"
RES="${1:-1024x768}"
IMG="${IMG:-output/xxri-disk.img}"
MON="$D/mon.sock"; QMP="$D/qmp.sock"; SER="$D/serial.log"
ps -eo pid,args | grep 'work/vm/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
sleep 2; rm -f "$MON" "$QMP" "$SER"
if [ -n "${ISO:-}" ]; then
  [ -f "$D/blank.img" ] || qemu-img create -f raw "$D/blank.img" 2G >/dev/null
  BOOT=(-boot d -cdrom output/XXRI-Lite.iso -drive file="$D/blank.img",format=raw,if=ide)
else
  BOOT=(-kernel work/iso/boot/vmlinuz -initrd output/core.gz
        -append "loglevel=3 xres=$RES ${APPEND:-}"
        -drive file="$IMG",format=raw,if=ide)
fi
setsid qemu-system-x86_64 -enable-kvm -m 2048 "${BOOT[@]}" \
  -vga std -display none -serial file:"$SER" \
  -device piix3-usb-uhci -device usb-tablet \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -audiodev none,id=snd0 -device AC97,audiodev=snd0 \
  -monitor unix:"$MON",server,nowait -qmp unix:"$QMP",server,nowait \
  >"$D/qemu.log" 2>&1 &
for i in $(seq 1 25); do [ -S "$QMP" ] && break; sleep 1; done
[ -S "$QMP" ] || { echo "QEMU did not start"; tail -3 "$D/qemu.log"; exit 1; }
echo "VM up: qmp=$QMP mon=$MON res=$RES img=${ISO:+LIVE ISO}${ISO:-$IMG}"
