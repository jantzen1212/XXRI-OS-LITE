#!/bin/bash
# build-iso.sh - build the XXRI OS Lite 2.6 ISO: one image for BIOS and 32-bit UEFI.
#
#   ./build-iso.sh [--no-validate] [--with-x64-efi] [-o output/NAME.iso]
#
# Boot architecture (docs/boot-2.6.md has the full rationale):
#
#   BIOS / legacy / CSM   upstream GRUB 2.14 i386-pc
#                         CD:  El Torito  boot/grub/i386-pc/eltorito.img
#                         USB: hybrid MBR (GRUB boot_hybrid.img) -> same core
#   IA32 (32-bit) UEFI    upstream GRUB 2.14 i386-efi as EFI/BOOT/BOOTIA32.EFI
#                         CD:  El Torito EFI entry -> appended ESP image
#                         USB: MBR partition 2, type 0xEF (the same ESP image)
#                         file copy (Rufus ISO mode etc.): /EFI/BOOT in the tree
#   x64 UEFI              NOT supported by default: BOOTIA32.EFI cannot run on
#                         64-bit firmware.  --with-x64-efi adds an experimental
#                         BOOTX64.EFI (GRUB x86_64-efi), which starts this i686
#                         kernel through the 32-bit boot protocol; see the doc.
#
# Every loader finds this medium by the marker file /.disk/<id> rather than by
# a volume label or the ISO9660 UUID, so the same core images work from a CD,
# a dd-written stick, and a stick whose files were copied to FAT32 (Rufus ISO
# mode).  GRUB places the initrd itself from the firmware memory map; XXRI adds
# no mem=, no initrd address and no other memory ceiling.
#
# Reproducibility: core.gz is packed from rootfs/ + rootfs.meta (modes, empty
# directories and mtimes git cannot store) in sorted order with fixed owners,
# and every timestamp in the ISO comes from SOURCE_DATE_EPOCH (the commit time
# by default), so two builds of one commit are byte-identical.
#
# Inputs:  rootfs/ rootfs.meta  work/iso/boot/vmlinuz  work/iso/cde/
#          build/grub/<platform>  (./tools/build-grub.sh)
# Outputs: output/XXRI-Lite-2.6.iso (+ .sha256), output/core.gz,
#          output/sources/  GPL Corresponding Source for the GRUB binaries
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
# Inputs can be overridden so another orchestrator (the PTXdist build in
# ptxdist/) assembles the ISO with this same code from its own outputs:
#   XXRI_ROOTFS    root tree to pack        XXRI_CORE_GZ  prebuilt core.gz (skip packing)
#   XXRI_ISOTREE   kernel + cde input tree  XXRI_GRUB     GRUB build (tools/build-grub.sh layout)
#   XXRI_SOURCES   GRUB source tarball dir  XXRI_OUT      output directory
ROOTFS="${XXRI_ROOTFS:-$HERE/rootfs}"
META="$HERE/rootfs.meta"
OUT="${XXRI_OUT:-$HERE/output}"
ISOTREE="${XXRI_ISOTREE:-$HERE/work/iso}"
G="${XXRI_GRUB:-$HERE/build/grub}"
SRCDIR="${XXRI_SOURCES:-$HERE/sources}"
VERSION="2.6"
ISO="$OUT/XXRI-Lite-$VERSION.iso"
VALIDATE=1 X64EFI=0
while [ $# -gt 0 ]; do
	case "$1" in
		--no-validate) VALIDATE=0 ;;
		--with-x64-efi) X64EFI=1 ;;
		-o) ISO=$(readlink -f "$2"); shift ;;
		*) echo "usage: $0 [--no-validate] [--with-x64-efi] [-o ISO]" >&2; exit 2 ;;
	esac
	shift
done
mkdir -p "$OUT"

need() { command -v "$1" >/dev/null || { echo "missing host tool: $1" >&2; exit 1; }; }
for t in xorriso cpio gzip mkfs.fat mcopy mksquashfs python3; do need "$t"; done
[ -d "$ROOTFS" ] || { echo "no rootfs at $ROOTFS" >&2; exit 1; }
[ -f "$META" ] || { echo "no $META (tools/rootfs-meta.py capture)" >&2; exit 1; }
[ -f "$ISOTREE/boot/vmlinuz" ] || { echo "no kernel at $ISOTREE/boot/vmlinuz" >&2; exit 1; }
PLATS="i386-pc i386-efi"; [ $X64EFI = 1 ] && PLATS="$PLATS x86_64-efi"
for p in $PLATS; do
	[ -d "$G/$p/usr/lib/grub/$p" ] || { echo "no GRUB $p build - run tools/build-grub.sh" >&2; exit 1; }
done
MKIMAGE="$G/i386-pc/usr/bin/grub-mkimage"
GRUB_VERSION=$("$MKIMAGE" --version | awk '{print $NF}')
T="$SRCDIR/grub-$GRUB_VERSION.tar.xz"
[ -f "$T" ] && [ -f "$T.sig" ] || { echo "no $T(.sig) - run tools/build-grub.sh" >&2; exit 1; }

# Timestamps come from the last commit that changed a build input, so a
# docs-only commit does not change the ISO.
: "${SOURCE_DATE_EPOCH:=$(git -C "$HERE" log -1 --format=%ct -- rootfs rootfs.meta build-iso.sh \
	tools/build-grub.sh tools/rootfs-meta.py 2>/dev/null || date +%s)}"
export SOURCE_DATE_EPOCH TZ=UTC LC_ALL=C
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

# ---------------------------------------------------------------- core.gz --
if [ -n "${XXRI_CORE_GZ:-}" ]; then
	echo ">> core.gz: prebuilt $XXRI_CORE_GZ"
	cp "$XXRI_CORE_GZ" "$OUT/core.gz"
else
	echo ">> core.gz from $ROOTFS (+ rootfs.meta, SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH)"
	mkdir "$W/rootfs"
	# Anything under rootfs/ would land in the initrd: never pack VCS or nested
	# worktree scratch (rootfs/.kilo once added 56 MB).
	tar -C "$ROOTFS" --exclude=./.kilo --exclude=./.git -cf - . | tar -C "$W/rootfs" -xf -
	python3 "$HERE/tools/rootfs-meta.py" apply "$W/rootfs" "$META"
	( cd "$W/rootfs" && find . | sort |
		cpio -o -H newc --quiet --reproducible -R 0:0 ) | gzip -9 -n > "$OUT/core.gz"
fi
ls -l "$OUT/core.gz"

# ------------------------------------------------------------- ISO layout --
R="$W/iso"
mkdir -p "$R/boot/grub" "$R/EFI/BOOT" "$R/.disk"
cp "$ISOTREE/boot/vmlinuz" "$R/boot/vmlinuz"
cp "$OUT/core.gz" "$R/boot/core.gz"
[ -d "$ISOTREE/cde" ] && cp -a "$ISOTREE/cde" "$R/cde"

# xxri-grub.tcz: what xxri-install-backend needs to put GRUB on a disk - the
# i686 GRUB utilities and the i386-pc / i386-efi module trees of the same
# 2.14 build.  Loaded on demand by the installer, not in onboot.lst.
[ -x "$G/i686-userland/sbin/grub-install" ] ||
	{ echo "no i686 GRUB utilities - run tools/build-grub.sh --target-userland" >&2; exit 1; }
X="$W/xxri-grub"
mkdir -p "$X/usr/local/sbin" "$X/usr/local/lib/grub" "$X/usr/local/share/doc/xxri-grub"
cp "$G"/i686-userland/sbin/* "$X/usr/local/sbin/"
# *.module / *.exec / gdb_grub are debug-symbol copies for gdb (15 MB of 18).
for p in i386-pc i386-efi; do
	tar -C "$G/$p/usr/lib/grub" --exclude='*.module' --exclude='*.exec' --exclude=gdb_grub \
		--exclude='*.py' --exclude=config.h -cf - "$p" | tar -C "$X/usr/local/lib/grub" -xf -
done
tar -xOf "$T" "grub-$GRUB_VERSION/COPYING" > "$X/usr/local/share/doc/xxri-grub/COPYING"
echo "GNU GRUB $GRUB_VERSION, unmodified upstream; source: see /boot/grub/SOURCE.txt on the XXRI medium" \
	> "$X/usr/local/share/doc/xxri-grub/SOURCE.txt"
find "$X" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
mkdir -p "$R/cde/optional"
mksquashfs "$X" "$R/cde/optional/xxri-grub.tcz" -noappend -all-root -no-xattrs \
	-comp xz >/dev/null   # timestamps: SOURCE_DATE_EPOCH (exported above)
( cd "$R/cde/optional" && md5sum xxri-grub.tcz > xxri-grub.tcz.md5.txt )

# The marker names this exact build, so a machine with two XXRI sticks, or an
# installed XXRI disk, still boots the files of the medium it started from.
ID=$(cat "$R/boot/vmlinuz" "$R/boot/core.gz" | sha256sum | cut -c1-16)
MARK="/.disk/xxri-lite-$VERSION-$ID"
: > "$R$MARK"
printf 'XXRI OS Lite %s (i686) %s\n' "$VERSION" "$ID" > "$R/.disk/info"

for p in $PLATS; do
	mkdir -p "$R/boot/grub/$p"
	cp "$G/$p/usr/lib/grub/$p"/*.mod "$G/$p/usr/lib/grub/$p"/*.lst "$R/boot/grub/$p/"
done
cp "$G/i386-pc/usr/lib/grub/i386-pc/boot.img" "$R/boot/grub/i386-pc/"

# GPLv3: the object code on this medium comes with its licence and a pointer
# to the Corresponding Source, which build-iso.sh also publishes next to the ISO.
tar -xOf "$T" "grub-$GRUB_VERSION/COPYING" > "$R/boot/grub/COPYING"
cat > "$R/boot/grub/SOURCE.txt" <<EOF
GNU GRUB $GRUB_VERSION - Copyright (C) Free Software Foundation, Inc.
Licence: GNU General Public License version 3 or later (see COPYING).

The GRUB boot images and modules on this medium were built without
modification from the upstream release tarball:
  grub-$GRUB_VERSION.tar.xz  sha256 $(sha256sum "$T" | cut -d' ' -f1)
  https://ftp.gnu.org/gnu/grub/grub-$GRUB_VERSION.tar.xz (+ .sig)
The Corresponding Source - that tarball, its signature, and the scripts that
configured, built and assembled the images (tools/build-grub.sh,
build-iso.sh) - is published in the sources/ directory next to this ISO.
EOF

# ----------------------------------------------------------- GRUB config --
# Boot codes are unchanged from 2.5 except vga=791: GRUB passes the video mode
# through gfxpayload, and the kernel still gets the linear framebuffer the
# boot splash draws into (vesafb on BIOS, efifb on UEFI).
BASE="vt.global_cursor_default=0 consoleblank=0 cde"
cat > "$R/boot/grub/grub.cfg" <<EOF
# XXRI OS Lite $VERSION - shared by BIOS (i386-pc) and UEFI (i386-efi) GRUB.
set default=0
set timeout=5
set menu_color_normal=light-gray/black
set menu_color_highlight=white/magenta
insmod part_msdos
insmod part_gpt
insmod iso9660
insmod fat
if [ "\$grub_platform" = "efi" ]; then
	insmod efi_gop
	insmod efi_uga
	set gfxpayload=keep
else
	insmod vbe
	insmod vga
	set gfxpayload=1024x768x32,1024x768x16,1024x768,auto
fi

menuentry "XXRI OS Lite $VERSION" --id xxri {
	linux /boot/vmlinuz quiet loglevel=3 $BASE
	initrd /boot/core.gz
}
menuentry "XXRI OS Lite $VERSION (slow USB devices)" --id xxri-waitusb {
	linux /boot/vmlinuz quiet loglevel=3 $BASE waitusb=5
	initrd /boot/core.gz
}
menuentry "XXRI OS Lite $VERSION (verbose boot)" --id xxri-verbose {
	linux /boot/vmlinuz loglevel=7 $BASE
	initrd /boot/core.gz
}
menuentry "XXRI OS Lite $VERSION (command line only)" --id xxri-text {
	linux /boot/vmlinuz loglevel=3 cde text
	initrd /boot/core.gz
}
if [ "\$grub_platform" = "efi" ]; then
	menuentry "UEFI firmware settings" --id uefi-firmware {
		fwsetup
	}
fi
menuentry "Reboot" --id reboot {
	reboot
}
menuentry "Power off" --id poweroff {
	halt
}
EOF

# ------------------------------------------------------ GRUB core images --
# Early config embedded in every core image: find THIS medium, then hand over
# to the shared grub.cfg.  If the search fails, root stays on the device the
# firmware booted, which is the right answer for a plain CD or dd'd stick.
cat > "$W/early.cfg" <<EOF
search --no-floppy --set=root --file $MARK
set prefix=(\$root)/boot/grub
EOF
COMMON="part_msdos part_gpt iso9660 fat search search_fs_file search_fs_uuid search_label
	normal configfile echo test linux boot reboot halt"
mk() { # platform format output modules...
	local p=$1 fmt=$2 out=$3; shift 3
	"$G/$p/usr/bin/grub-mkimage" -d "$G/$p/usr/lib/grub/$p" -O "$fmt" \
		-p /boot/grub -c "$W/early.cfg" -o "$out" "$@"
}
echo ">> GRUB $GRUB_VERSION core images"
mk i386-pc i386-pc-eltorito "$R/boot/grub/i386-pc/eltorito.img" biosdisk $COMMON
mk i386-efi i386-efi "$R/EFI/BOOT/BOOTIA32.EFI" $COMMON efi_gop efi_uga efifwsetup all_video
[ $X64EFI = 1 ] &&
	mk x86_64-efi x86_64-efi "$R/EFI/BOOT/BOOTX64.EFI" $COMMON efi_gop efi_uga efifwsetup all_video
ls -l "$R/boot/grub/i386-pc/eltorito.img" "$R"/EFI/BOOT/*.EFI | awk '{print "   " $5, $NF}'

# EFI system partition image: FAT, holding exactly the EFI/BOOT loaders.  It is
# both the El Torito EFI boot image and MBR partition 2 (type 0xEF).
kib=$(du -sk "$R/EFI" | cut -f1); kib=$(( (kib + 512 + 31) / 32 * 32 ))
mkfs.fat -C -n XXRI_EFI -i "${ID:0:8}" --invariant "$W/efiboot.img" "$kib" >/dev/null
# mcopy -s -m copies the tree with its (SOURCE_DATE_EPOCH) mtimes; mmd would
# stamp the directories with the wall clock and break reproducibility.
find "$R/EFI" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
mcopy -s -m -i "$W/efiboot.img" "$R/EFI" ::/

# ---------------------------------------------------------------- xorriso --
find "$R" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
echo ">> $ISO"
rm -f "$ISO"
xorriso -as mkisofs -iso-level 3 -full-iso9660-filenames -joliet -joliet-long \
	-rational-rock -volid XXRI_LITE -appid "XXRI OS Lite $VERSION" \
	-publisher "XXRI OS Team" -preparer "build-iso.sh + GRUB $GRUB_VERSION" \
	--grub2-mbr "$G/i386-pc/usr/lib/grub/i386-pc/boot_hybrid.img" \
	-partition_offset 16 --mbr-force-bootable \
	-append_partition 2 0xef "$W/efiboot.img" \
	-c boot/grub/boot.cat \
	-b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 \
		-boot-info-table --grub2-boot-info \
	-eltorito-alt-boot \
	-e --interval:appended_partition_2:all:: -no-emul-boot \
	-o "$ISO" "$R" 2>"$W/xorriso.log" ||
	{ cat "$W/xorriso.log" >&2; exit 1; }
( cd "$(dirname "$ISO")" && sha256sum "$(basename "$ISO")" > "$(basename "$ISO").sha256" )
ls -l "$ISO"; cat "$ISO.sha256"

# ------------------------------------------------ Corresponding Source --
mkdir -p "$OUT/sources"
cp -p "$T" "$T.sig" "$OUT/sources/"
tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 -C "$HERE" \
	-czf "$OUT/sources/xxri-grub-build-scripts.tar.gz" tools/build-grub.sh build-iso.sh
cp "$R/boot/grub/SOURCE.txt" "$OUT/sources/README-GRUB.txt"

if [ $VALIDATE = 1 ]; then
	echo ">> Validating"
	"$HERE/validate-release.sh" "$ISO" --boot
fi
