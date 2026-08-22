#!/bin/bash
# inject-hook.sh FILE NAME [IMG] - put a QA session hook into the image, in
# BOTH /etc/skel/.X.d (used on a first boot) and /home/xxri/.X.d (used by an
# image that has already been booted once).  Forgetting the second one silently
# runs the previous version of the hook.
set -eu
F="$1"; N="$2"; IMG="${3:-/home/jantzen/xxri-build/output/xxri-disk.img}"
e2fsck -fy "$IMG" >/dev/null 2>&1 || true
for d in /etc/skel/.X.d /home/xxri/.X.d; do
  debugfs -R "ls $d" "$IMG" >/dev/null 2>&1 || continue
  debugfs -w "$IMG" >/dev/null 2>&1 <<EOS || true
cd $d
rm $N
write $F $N
sif $N mode 0100644
sif $N uid $([ "$d" = /etc/skel/.X.d ] && echo 0 || echo 1001)
sif $N gid $([ "$d" = /etc/skel/.X.d ] && echo 0 || echo 50)
EOS
  echo "injected $N -> $d"
done
