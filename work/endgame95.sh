#!/bin/bash
B=/home/jantzen/xxri-build; D=/home/jantzen/.xxri-testenv/q; cd "$B"
while pgrep -f "python3 .*gen-catalog" >/dev/null; do sleep 20; done
echo "=== catalog ready ==="
python3 - <<'PY'
import json
R='rootfs/usr/local/share/xxri-store/repository'
s=json.load(open(R+'/sections.json')); print('featured:', s['featured'])
rows=[l.rstrip('\n').split('\t') for l in open(R+'/index.tsv')]
i6=[r[0] for r in rows if len(r)>15 and r[15]=='1' and 'i686' in r[3].split()]
print('installable+verified:', len(i6), i6)
PY
chmod u+r rootfs/usr/bin/sudo rootfs/usr/sbin/visudo 2>/dev/null
./build-core.sh >/tmp/claude-1000/e95-core.log 2>&1 && echo "core.gz + ISO rebuilt"
./build-writable-image.sh >/tmp/claude-1000/e95-img.log 2>&1 && echo "disk image rebuilt"
ls -l output/core.gz output/XXRI-Lite.iso output/xxri-disk.img | awk '{printf "  %-26s %s\n",$9,$5}'

# --- device verification: install + launch every verified app -------------
cat > work/e95-hook.sh <<'EOF'
( LOG="$HOME/e95.log"; DOCK=/usr/local/tce.icons; MENU="$HOME/.wmx/Applications"
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  echo "=== Phase 9.5 device verification $(date) ===" > "$LOG"
  echo "bash present: $(command -v bash || echo NO)" >> "$LOG"
  ok=0; n=0
  for ID in appimagetool sponge256sum mangbandclient python fre-ac windows2usb; do
    n=$((n+1)); echo "" >> "$LOG"; echo "--- [$n] $ID ---" >> "$LOG"
    xxri-store install "$ID" >> "$LOG" 2>&1
    reg="$(xxri-app info "$ID" 2>/dev/null)"; nm="$(echo "$reg" | sed -n 's/^NAME=//p')"
    echo "  $nm $(echo "$reg" | sed -n 's/^VERSION=//p') dock:$(grep -c "exec xxri-app launch $ID" $DOCK 2>/dev/null) menu:$({ m=$(echo "$nm"|tr -d ' '); [ -f "$MENU/$m" ] && echo yes || echo NO; })" >> "$LOG"
    xxri-app launch "$ID" >> "$LOG" 2>&1; sleep 7
    rc="$(grep "exit .* for .*$ID" /tmp/xxri-app.log 2>/dev/null | tail -1 | sed 's/.*exit \([0-9]*\) .*/\1/')"
    if [ -z "$rc" ] || [ "$rc" = 0 ]; then echo "  launch: OK (rc=${rc:-running})" >> "$LOG"; ok=$((ok+1))
    else echo "  launch: FAILED rc=$rc" >> "$LOG"; fi
    pkill -f "$ID" 2>/dev/null
  done
  echo "" >> "$LOG"; xxri-app list >> "$LOG" 2>&1
  echo "RESULT: $ok/$n launched cleanly" >> "$LOG"; echo "=== E95 DONE ===" >> "$LOG"; sync
) >/dev/null 2>&1 &
EOF
cp --reflink=auto output/xxri-disk.img /tmp/claude-1000/e95.img
e2fsck -fy /tmp/claude-1000/e95.img >/dev/null 2>&1
debugfs -w /tmp/claude-1000/e95.img >/dev/null 2>&1 <<EOF
write $B/work/e95-hook.sh /etc/skel/.X.d/e95
EOF
for p in $(ps -eo pid,comm | awk '$2=="qemu-system-x86"{print $1}'); do kill -9 $p 2>/dev/null; done
sleep 3; rm -f $D/e95-mon.sock
setsid qemu-system-x86_64 -enable-kvm -m 1024 -kernel work/iso/boot/vmlinuz -initrd output/core.gz \
  -append "loglevel=3" -drive file=/tmp/claude-1000/e95.img,format=raw -vga std -display none \
  -netdev user,id=n0 -device e1000,netdev=n0 -monitor unix:$D/e95-mon.sock,server,nowait >$D/e95.log 2>&1 &
for i in $(seq 1 25); do [ -S $D/e95-mon.sock ] && break; sleep 1; done
sleep 300
rm -f $D/e95.ppm; echo "screendump $D/e95.ppm" | socat - UNIX-CONNECT:$D/e95-mon.sock 2>/dev/null; sleep 3
magick $D/e95.ppm $D/shots/phase95-final.png 2>/dev/null && echo "screenshot: $(wc -c < $D/shots/phase95-final.png) bytes"
echo "system_powerdown" | socat - UNIX-CONNECT:$D/e95-mon.sock 2>/dev/null; sleep 12
for p in $(ps -eo pid,comm | awk '$2=="qemu-system-x86"{print $1}'); do kill -9 $p 2>/dev/null; done
sleep 2; e2fsck -fy /tmp/claude-1000/e95.img >/dev/null 2>&1
debugfs -R "dump /home/xxri/e95.log $D/e95-result.log" /tmp/claude-1000/e95.img 2>/dev/null
echo "--- device log ---"; cat $D/e95-result.log 2>/dev/null
echo ENDGAME95_DONE
