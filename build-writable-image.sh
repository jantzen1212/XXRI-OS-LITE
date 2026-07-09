#!/bin/sh
# build-writable-image.sh - build a bootable, WRITABLE ext4 disk image that
# is the installed XXRI system.
#
# It bakes the base rootfs PLUS the desktop extensions permanently onto the
# disk (via unsquashfs), so the installed system has NO cde / TCE loop-mount
# pipeline at boot -- files simply exist on a writable filesystem, the way a
# normal distribution works.
#
# The whole staging tree is assembled INSIDE one fakeroot session so that
# root ownership AND setuid bits survive into the image.  This matters: the
# desktop server Xvesa ships setuid-root and will not start for the tc user
# (VESA needs I/O-port / /dev/mem access) if that bit is lost -- startx then
# hangs with "failed in waitforX".  mke2fs -d writes the populated ext4 with
# no loopback mount and no sudo.
#
# Usage: ./build-writable-image.sh [SIZE_MB]     (default 1024)
#
# NOTE on sudo: usr/bin/sudo and usr/sbin/visudo have no owner-read bit in
# the extracted rootfs, so a non-root build cannot read them and they are
# omitted (the desktop's wbar/setupdesktop then print "sudo: not found").
# Run this script as root, or `chmod u+r rootfs/usr/bin/sudo
# rootfs/usr/sbin/visudo` first, to include them.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOTFS="$HERE/rootfs"
CDE="$HERE/work/iso/cde"
OUT="$HERE/output"
IMG="$OUT/xxri-disk.img"
SIZE="${1:-1024}"
LABEL="XXRIROOT"
mkdir -p "$OUT"

command -v fakeroot   >/dev/null 2>&1 || { echo "need fakeroot" >&2; exit 1; }
command -v mke2fs     >/dev/null 2>&1 || { echo "need mke2fs (e2fsprogs)" >&2; exit 1; }
command -v unsquashfs >/dev/null 2>&1 || echo "!! unsquashfs missing: desktop extensions will NOT be baked in" >&2

STAGE=$(mktemp -d)
STAGESH=$(mktemp)
trap 'rm -rf "$STAGE" "$STAGESH"' EXIT

# Everything that must end up root-owned / setuid runs in the fakeroot
# session below.  Order matters: chown the base rootfs to root FIRST (chown
# clears setuid bits), THEN unsquashfs the extensions (fakeroot keeps their
# root ownership + setuid), THEN re-apply the base setuid bits.
cat > "$STAGESH" <<STAGESCRIPT
set -e
STAGE="$STAGE"; ROOTFS="$ROOTFS"; CDE="$CDE"; IMG="$IMG"; LABEL="$LABEL"; SIZE="$SIZE"

echo ">> 1 copy base rootfs"
cp -a "\$ROOTFS/." "\$STAGE/" 2>/dev/null || true    # sudo/visudo may be unreadable
chown -R 0:0 "\$STAGE"

echo ">> 2 bake desktop extensions (setuid preserved under fakeroot)"
if command -v unsquashfs >/dev/null 2>&1 && [ -f "\$CDE/onboot.lst" ]; then
	while read ext; do
		[ -n "\$ext" ] || continue
		tcz="\$CDE/optional/\$ext"
		[ -f "\$tcz" ] && unsquashfs -n -f -d "\$STAGE" "\$tcz" >/dev/null 2>&1 || true
	done < "\$CDE/onboot.lst"
fi

echo ">> 3 re-apply base setuid bits (chown cleared them)"
chmod 4755 "\$STAGE/bin/busybox.suid" 2>/dev/null || true
chmod 4755 "\$STAGE/usr/bin/sudo"     2>/dev/null || true

echo ">> 4 seed desktop config (Xvesa/flwm/wbar tce.installed equivalents)"
mkdir -p "\$STAGE/etc/sysconfig"
[ -x "\$STAGE/usr/local/bin/Xvesa" ] && echo Xvesa > "\$STAGE/etc/sysconfig/Xserver"
[ -x "\$STAGE/usr/local/bin/flwm" ]  && echo flwm  > "\$STAGE/etc/sysconfig/desktop"
[ -x "\$STAGE/usr/local/bin/wbar" ]  && echo wbar  > "\$STAGE/etc/sysconfig/icons"

echo ">> 5 runtime dirs, persistent /tce, writable fstab, dev nodes"
for d in proc sys dev tmp run mnt tce/optional tce/ondemand; do mkdir -p "\$STAGE/\$d"; done
chmod 1777 "\$STAGE/tmp"
: > "\$STAGE/tce/onboot.lst"
rm -f "\$STAGE/etc/sysconfig/tcedir"; ln -sf /tce "\$STAGE/etc/sysconfig/tcedir"
rm -f "\$STAGE/usr/local/tce.installed/"* 2>/dev/null || true
rm -f "\$STAGE/var/log/autologin" "\$STAGE/etc/sysconfig/backup_device" \
      "\$STAGE/etc/sysconfig/noautologin" "\$STAGE/etc/sysconfig/text" 2>/dev/null || true
cat > "\$STAGE/etc/fstab" <<EOF
# /etc/fstab - installed XXRI system (persistent; not regenerated at boot)
proc            /proc        proc    defaults          0 0
sysfs           /sys         sysfs   defaults          0 0
devpts          /dev/pts     devpts  defaults          0 0
tmpfs           /dev/shm     tmpfs   defaults          0 0
tmpfs           /tmp         tmpfs   defaults          0 0
tmpfs           /run         tmpfs   defaults          0 0
LABEL=\$LABEL    /            ext4    defaults,noatime  0 1
EOF
[ -e "\$STAGE/dev/console" ] || mknod "\$STAGE/dev/console" c 5 1
[ -e "\$STAGE/dev/null" ]    || mknod "\$STAGE/dev/null"    c 1 3
[ -e "\$STAGE/dev/zero" ]    || mknod "\$STAGE/dev/zero"    c 1 5
[ -e "\$STAGE/dev/tty" ]     || mknod "\$STAGE/dev/tty"     c 5 0

echo ">> 6 write ext4 image (label \$LABEL, \${SIZE}M)"
rm -f "\$IMG"
mke2fs -q -F -t ext4 -L "\$LABEL" -d "\$STAGE" "\$IMG" \${SIZE}M
STAGESCRIPT

fakeroot sh "$STAGESH"

echo ">> Done: $IMG"
echo
echo "Boot it (no root= needed; /init auto-detects LABEL=$LABEL):"
echo "  qemu-system-x86_64 -enable-kvm -m 1024 \\"
echo "    -kernel $HERE/work/iso/boot/vmlinuz -initrd $OUT/core.gz \\"
echo "    -append 'loglevel=3' -drive file=$IMG,format=raw"
echo
echo "Or write it to a real disk/USB (DESTROYS that disk):"
echo "  sudo dd if=$IMG of=/dev/sdX bs=4M status=progress conv=fsync"
