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
STAGE="$STAGE"; ROOTFS="$ROOTFS"; CDE="$CDE"; IMG="$IMG"; LABEL="$LABEL"; SIZE="$SIZE"; ASSETS="$HERE/assets"; HERE="$HERE"

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

echo ">> 2b runtime overrides (these must WIN over the extensions)"
# Step 1 copies rootfs/, then step 2 unsquashes every extension over it with
# -f, so a .tcz always overwrites a file of the same name from rootfs/.  Any
# component we deliberately replace upstream - a library Tiny Core builds
# incorrectly, a loader it does not ship - therefore has to be applied HERE,
# after the extensions have been laid down.
if [ -d "\$HERE/rootfs-overrides" ]; then
	cp -a "\$HERE/rootfs-overrides/." "\$STAGE/"
	chown -R 0:0 "\$STAGE/usr" 2>/dev/null || true
	echo "   applied \$(find "\$HERE/rootfs-overrides" -type f | wc -l) override files"
fi

echo ">> 2c de-brand the baked desktop (wallpaper logo + launcher/menu text)"
# Replace the "core" wallpaper logo with the xxri wordmark (single-point:
# skel/.setbackground centers /usr/local/share/pixmaps/logo.png).
if [ -f "\$ASSETS/logo.png" ] && [ -f "\$STAGE/usr/local/share/pixmaps/logo.png" ]; then
	cp -f "\$ASSETS/logo.png" "\$STAGE/usr/local/share/pixmaps/logo.png"
fi
# Rebrand visible Tiny Core text in launcher .desktop files, menus and the
# fltk menu config (Name=/comments only; Exec=/binaries are left intact).
find "\$STAGE/usr/local/share/applications" "\$STAGE/usr/local/share/flwm" \
     "\$STAGE/etc/skel" -type f 2>/dev/null | while read df; do
	case "\$df" in
		*.desktop|*.menu|*menu*|*.jwmrc*|*flwm*)
			sed -i 's/Tiny Core Linux/xxri OS Lite/g; s/TinyCore/xxri OS Lite/g; s/Tiny Core/xxri OS Lite/g; s/^Name=tc-wbarconf/Name=Wbar Config/' "\$df" 2>/dev/null || true
		;;
	esac
done
# The desktop.sh username fallback ("tc") -> xxri (only used if xxri-user/tcuser is absent).
[ -f "\$STAGE/usr/local/bin/desktop.sh" ] && \
	sed -i 's/USER="tc"/USER="xxri"/g' "\$STAGE/usr/local/bin/desktop.sh" 2>/dev/null || true

echo ">> 2d apply the xxri design system over the baked desktop"
# Phase 4: the design-system files live in rootfs/ but share paths with
# files inside the desktop extensions (skel dotfiles, wbar's dot.wbar, the
# launcher pixmaps), so the unsquashfs in step 2 just clobbered them.
# Re-apply the rootfs versions; at live-ISO boot the same files win
# naturally because the extension loader never overwrites existing files.
cp -a "\$ROOTFS/etc/skel/." "\$STAGE/etc/skel/"
cp -f "\$ROOTFS/usr/local/share/wbar/dot.wbar" "\$STAGE/usr/local/share/wbar/dot.wbar" 2>/dev/null || true
cp -f "\$ROOTFS/usr/local/bin/wbar_setup.sh" "\$STAGE/usr/local/bin/wbar_setup.sh" 2>/dev/null || true
# desktop.sh: xxri build the menu only (dot.wbar is the sole dock source).
cp -f "\$ROOTFS/usr/local/bin/desktop.sh" "\$STAGE/usr/local/bin/desktop.sh" 2>/dev/null || true
# wbar.sh: xxri launch wbar with no fragile .wbarconf string surgery (Phase 8.2).
cp -f "\$ROOTFS/usr/local/bin/wbar.sh" "\$STAGE/usr/local/bin/wbar.sh" 2>/dev/null || true
chmod 0755 "\$STAGE/usr/local/bin/wbar.sh" 2>/dev/null || true
# Phase 8.1: xxri Settings fully replaces the Tiny Core control panel and its
# stand-alone config launchers - drop their .desktop files and the duplicate
# gear pixmap so they never reach the dock or the Applications/SystemTools menu.
# Phase 9.1: the XXRI Store is the ONLY software store - also drop the Tiny Core
# package browser GUI (apps/appbrowser/tce) so there is no second store icon.
for junk in cpanel tc-cpanel tc-config appsaudit services settime tc-wbarconf \
            apps appbrowser tce tce-ab appsrepo; do
	rm -f "\$STAGE/usr/local/share/applications/tinycore-\$junk.desktop" \
	      "\$STAGE/usr/local/share/applications/\$junk.desktop"
done
rm -f "\$STAGE/usr/local/share/pixmaps/cpanel.png" \
      "\$STAGE/usr/local/bin/apps" "\$STAGE/usr/local/bin/appbrowser"
# This ext4 image IS an installed system - the installer belongs to the Live
# ISO only.  Strip its launcher, icon, GUI binary and auto-start hook.
rm -f "\$STAGE/usr/local/share/applications/xxri-installer.desktop" \
      "\$STAGE/usr/local/share/pixmaps/xxri-installer.png" \
      "\$STAGE/usr/local/bin/xxri-installer" \
      "\$STAGE/etc/skel/.X.d/xxri-installer-live" 2>/dev/null
for px in aterm editor apps flrun mnttool exittc gear core; do
	[ -f "\$ROOTFS/usr/local/share/pixmaps/\$px.png" ] && \
		cp -f "\$ROOTFS/usr/local/share/pixmaps/\$px.png" "\$STAGE/usr/local/share/pixmaps/\$px.png"
done
chown -R 0:0 "\$STAGE/etc/skel"

echo ">> 2e build the TLS CA bundle (ca-certificates.tcz normally does this at load)"
# The baked image unsquashes extensions directly, so the ca-certificates
# tce.installed script never runs and /usr/local/etc/ssl/certs/ca-certificates.crt
# is missing - without it GNU wget cannot verify HTTPS and the Store cannot
# download any real AppImage.  Concatenate the shipped certs, exactly as
# update-ca-certificates would.
CADIR="\$STAGE/usr/local/share/ca-certificates"
if [ -d "\$CADIR" ]; then
	mkdir -p "\$STAGE/usr/local/etc/ssl/certs"
	find "\$CADIR" -name '*.crt' -exec cat {} + > "\$STAGE/usr/local/etc/ssl/certs/ca-certificates.crt" 2>/dev/null
	ln -sf /usr/local/etc/ssl/certs/ca-certificates.crt "\$STAGE/usr/local/etc/ssl/cacert.pem"
	ln -sf /usr/local/etc/ssl/certs/ca-certificates.crt "\$STAGE/usr/local/etc/ssl/ca-bundle.crt"
	echo "   CA bundle: \$(grep -c 'BEGIN CERTIFICATE' "\$STAGE/usr/local/etc/ssl/certs/ca-certificates.crt" 2>/dev/null) certificates"
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
# Record every extension baked into the image as already installed. The files
# were unsquashed straight into the rootfs in step 2, so this is simply the
# truth - and it is what lets tce-load resolve a new extension's dependencies
# against what the OS already ships instead of re-downloading the whole
# desktop stack when the Store installs a native app.
mkdir -p "\$STAGE/usr/local/tce.installed" "\$STAGE/usr/local/share/xxri"
rm -f "\$STAGE/usr/local/tce.installed/"* 2>/dev/null || true
: > "\$STAGE/usr/local/share/xxri/baked-extensions.lst"
if [ -f "\$CDE/onboot.lst" ]; then
	while read ext; do
		[ -n "\$ext" ] || continue
		[ -f "\$CDE/optional/\$ext" ] || continue
		: > "\$STAGE/usr/local/tce.installed/\${ext%.tcz}"
		echo "\${ext%.tcz}" >> "\$STAGE/usr/local/share/xxri/baked-extensions.lst"
	done < "\$CDE/onboot.lst"
fi
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
