#!/bin/bash
# QEMU dock + settings verification at multiple resolutions.
cd /home/jantzen/xxri-build
OUT=/home/jantzen/.xxri-testenv/qemu; mkdir -p "$OUT"
IMG=output/xxri-disk.img
KERNEL=work/iso/boot/vmlinuz
INITRD=output/core.gz
dump(){ echo "screendump $1" | socat - UNIX-CONNECT:"$2" >/dev/null 2>&1; }
boot_cap(){ # RES(WxH|"") IMGFILE  CAPSPEC...  where CAPSPEC=delay:outbasename
  local res="$1" img="$2"; shift 2
  local mon=$(mktemp -u /tmp/qmon.XXXXXX)
  local append="loglevel=3"; [ -n "$res" ] && append="$append xres=$res"
  qemu-system-x86_64 -enable-kvm -m 1024 -kernel "$KERNEL" -initrd "$INITRD" \
    -append "$append" -drive file="$img",format=raw -vga std \
    -display none -monitor unix:"$mon",server,nowait >/dev/null 2>&1 &
  local qp=$!
  local prev=0
  for spec in "$@"; do
    local d="${spec%%:*}" name="${spec##*:}"
    sleep $((d - prev)); prev=$d
    dump "$OUT/$name.ppm" "$mon"
    magick "$OUT/$name.ppm" "$OUT/$name.png" 2>/dev/null && rm -f "$OUT/$name.ppm"
    echo "captured $name at ${d}s"
  done
  kill $qp 2>/dev/null; wait $qp 2>/dev/null; rm -f "$mon"
}
echo "=== dock boots (clean image) ==="
boot_cap "1024x768"  "$IMG" 34:dock-1024x768
boot_cap "1280x720"  "$IMG" 34:dock-1280x720
boot_cap "1280x768"  "$IMG" 34:dock-1280x768
boot_cap "1366x768"  "$IMG" 34:dock-1366x768
echo "=== settings boot (hooked copy) ==="
cp "$IMG" output/xxri-hook.img
e2fsck -fy output/xxri-hook.img >/dev/null 2>&1
printf '(sleep 16; xxri-settings wifi) &\n' > /tmp/xhook
debugfs -w -R "write /tmp/xhook /etc/skel/.X.d/zz-verify" output/xxri-hook.img >/dev/null 2>&1
boot_cap "1024x768" output/xxri-hook.img 40:settings-wifi
rm -f output/xxri-hook.img
echo "ALL DONE"
ls -la "$OUT"/*.png
