#!/bin/bash
# vm-shot.sh NAME - screendump the held VM
set -u
D=/home/jantzen/xxri-build/work/vm
rm -f "$D/s.ppm"
echo "screendump $D/s.ppm" | socat - UNIX-CONNECT:"$D/mon.sock" >/dev/null 2>&1
sleep 2
magick "$D/s.ppm" "$D/shots/$1.png" 2>/dev/null && echo "SHOT $1 $(identify -format '%wx%h' "$D/shots/$1.png")"
