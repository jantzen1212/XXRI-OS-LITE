#!/bin/bash
# xxri-mkiso.sh - assemble the XXRI OS Lite ISO: one image for BIOS and 32-bit
# UEFI.  Called by rules/image-xxri-iso.make with everything already built:
#
#   xxri-mkiso.sh VERSION ISO CORE_GZ VMLINUZ CDE_DIR GRUB_PREFIX GRUB_TARBALL
#                 [--ucode UCODE_DIR] [--with-x64-efi]
#
#   CDE_DIR      the cde/ tree (onboot.lst, copy2fs.lst, xbase.lst, optional/*.tcz)
#   GRUB_PREFIX  host-xxri-grub's install prefix: <GRUB_PREFIX>/<platform>/usr/{bin,lib/grub}
#   UCODE_DIR    early CPU microcode (rules/xxri-ucode.make): intel-ucode.img,
#                amd-ucode.img and their LICENSE.* files, placed in /boot
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
X64EFI=0 UCODE=""
shift 7
while [ $# -gt 0 ]; do
	case "$1" in
	--with-x64-efi) X64EFI=1 ;;
	--ucode) UCODE=$2; shift ;;
	*) echo "xxri-mkiso.sh: unknown option $1" >&2; exit 1 ;;
	esac
	shift
done
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
UCODE_IMGS=""
if [ -n "$UCODE" ]; then
	for u in intel amd; do
		cp "$UCODE/$u-ucode.img" "$UCODE/LICENSE.$u-ucode" "$R/boot/"
		UCODE_IMGS="$UCODE_IMGS /boot/$u-ucode.img"
	done
	UCODE_IMGS=${UCODE_IMGS# }
fi
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
# No vga=791: GRUB sets the video mode through gfxpayload, and the kernel
# still gets the linear framebuffer the boot splash draws into (vesafb on
# BIOS, efifb on UEFI).
BASE="vt.global_cursor_default=0 consoleblank=0 cde"
INITRD=/boot/core.gz
[ -n "$UCODE_IMGS" ] && INITRD='$xxri_ucode /boot/core.gz'
{
cat <<EOF
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
EOF
# CPU microcode: the kernel applies it on the boot CPU before anything else
# runs, from uncompressed archives GRUB puts in front of core.gz on the same
# initrd line.  Only the image of this CPU's vendor is read - the Intel one
# is 15 MB, as much again as core.gz - chosen by the processor manufacturer
# in the firmware's SMBIOS table (type 4, the string at offset 7).  Without
# SMBIOS, or with a name that says neither vendor, both are loaded and the
# kernel takes the one that matches: never less than the CPU needs.
# ($xxri_ucode is unquoted on the initrd lines: GRUB splits it into words.)
[ -n "$UCODE_IMGS" ] && cat <<EOF

# CPU microcode for this CPU's vendor (see xxri-mkiso.sh)
set xxri_ucode="$UCODE_IMGS"
insmod smbios
insmod regexp
if smbios --type 4 --get-string 7 --set xxri_cpu; then
	if regexp '[Ii]ntel' "\$xxri_cpu"; then
		set xxri_ucode=/boot/intel-ucode.img
	elif regexp 'AMD|[Aa]dvanced [Mm]icro' "\$xxri_cpu"; then
		set xxri_ucode=/boot/amd-ucode.img
	fi
fi
export xxri_ucode
EOF
cat <<EOF

menuentry "XXRI OS Lite $VERSION" --id xxri {
	linux /boot/vmlinuz quiet loglevel=3 $BASE
	initrd $INITRD
}
menuentry "XXRI OS Lite $VERSION (slow USB devices)" --id xxri-waitusb {
	linux /boot/vmlinuz quiet loglevel=3 $BASE waitusb=5
	initrd $INITRD
}
# nomodeset: no KMS driver binds (i915, xe, amdgpu, radeon, nouveau and the
# virtual GPUs all refuse); the desktop runs on the boot loader's framebuffer
# (Xfbdev) - for a GPU whose driver hangs or leaves the screen dark.
menuentry "XXRI OS Lite $VERSION (safe graphics)" --id xxri-safegfx {
	linux /boot/vmlinuz quiet loglevel=3 $BASE nomodeset
	initrd $INITRD
}
menuentry "XXRI OS Lite $VERSION (verbose boot)" --id xxri-verbose {
	linux /boot/vmlinuz loglevel=7 $BASE
	initrd $INITRD
}
menuentry "XXRI OS Lite $VERSION (command line only)" --id xxri-text {
	linux /boot/vmlinuz loglevel=3 cde text
	initrd $INITRD
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
} > "$R/boot/grub/grub.cfg"

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
