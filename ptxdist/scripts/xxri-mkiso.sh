#!/bin/bash
# xxri-mkiso.sh - assemble the XXRI OS Lite ISO: one image for BIOS and 32-bit
# UEFI.  Called by rules/image-xxri-iso.make with everything already built:
#
#   xxri-mkiso.sh VERSION ISO CORE_GZ VMLINUZ CDE_DIR GRUB_PREFIX GRUB_TARBALL [--with-x64-efi]
#
#   CDE_DIR      the cde/ tree (onboot.lst, copy2fs.lst, xbase.lst, optional/*.tcz)
#   GRUB_PREFIX  host-xxri-grub's install prefix: <GRUB_PREFIX>/<platform>/usr/{bin,lib/grub}
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
#                         BOOTX64.EFI (GRUB x86_64-efi), see the doc.
#
# Every loader finds this medium by the marker file /.disk/<id> rather than by
# a volume label or the ISO9660 UUID, so the same core images work from a CD,
# a dd-written stick, and a stick whose files were copied to FAT32 (Rufus ISO
# mode).  GRUB places the initrd itself from the firmware memory map; XXRI adds
# no mem=, no initrd address and no other memory ceiling.
#
# Every timestamp comes from SOURCE_DATE_EPOCH (set by PTXdist), so two builds
# of one tree are byte-identical.
set -euo pipefail
VERSION=$1 ISO=$2 CORE=$3 KERNEL=$4 CDE=$5 G=$6 T=$7
X64EFI=0; [ "${8:-}" = --with-x64-efi ] && X64EFI=1
: "${SOURCE_DATE_EPOCH:?SOURCE_DATE_EPOCH must be set}"
export TZ=UTC LC_ALL=C
PLATS="i386-pc i386-efi"; [ $X64EFI = 1 ] && PLATS="$PLATS x86_64-efi"
for p in $PLATS; do
	[ -d "$G/$p/usr/lib/grub/$p" ] || { echo "no GRUB $p build in $G" >&2; exit 1; }
done
GRUB_VERSION=$("$G/i386-pc/usr/bin/grub-mkimage" --version | awk '{print $NF}')
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

# ------------------------------------------------------------- ISO layout --
R="$W/iso"
mkdir -p "$R/boot/grub" "$R/EFI/BOOT" "$R/.disk"
cp "$KERNEL" "$R/boot/vmlinuz"
cp "$CORE" "$R/boot/core.gz"
cp -rL "$CDE" "$R/cde"

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
# to the Corresponding Source (published next to the ISO by the image rule).
tar -xOf "$T" "grub-$GRUB_VERSION/COPYING" > "$R/boot/grub/COPYING"
cat > "$R/boot/grub/SOURCE.txt" <<EOF
GNU GRUB $GRUB_VERSION - Copyright (C) Free Software Foundation, Inc.
Licence: GNU General Public License version 3 or later (see COPYING).

The GRUB boot images and modules on this medium were built without
modification from the upstream release tarball:
  grub-$GRUB_VERSION.tar.xz  sha256 $(sha256sum "$T" | cut -d' ' -f1)
  https://ftp.gnu.org/gnu/grub/grub-$GRUB_VERSION.tar.xz (+ .sig)
The Corresponding Source - that tarball, its signature, and the PTXdist
rules and scripts that configured, built and assembled the images - is
published in the sources/ directory next to this ISO.
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
rm -f "$ISO"
xorriso -as mkisofs -iso-level 3 -full-iso9660-filenames -joliet -joliet-long \
	-rational-rock -volid XXRI_LITE -appid "XXRI OS Lite $VERSION" \
	-publisher "XXRI OS Team" -preparer "PTXdist + GRUB $GRUB_VERSION" \
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
cat "$ISO.sha256"
