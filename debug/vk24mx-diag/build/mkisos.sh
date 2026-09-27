#!/bin/bash
# Build the VK24MX diagnostic ISOs from the released ISO.
#
#   mkisos.sh RELEASE_ISO OUTDIR            tester ISOs
#   mkisos.sh RELEASE_ISO OUTDIR test ENTRY QEMU-validation ISOs: serial console,
#                                            auto-boot ENTRY
#
# Every ISO carries the release's vmlinuz/core.gz/cde unchanged, plus
# /boot/xdiag.gz (the checker, loaded ahead of core.gz) and two more copies of
# the same kernel whose initrd_addr_max is lowered, so the loader is forced to
# put core.gz somewhere else:
#     /boot/vmlinuz          0x7FFFFFFF  -> ~2 GB, kernel relocates it
#     /boot/midmem/vmlinuz   0x3FFFFFFF  -> ~1 GB, kernel still relocates it
#     /boot/lowmem/vmlinuz   0x2FFFFFFF  -> <768 MB, used in place
# Comparing those three separates "this physical address is bad" from
# "the high-memory relocation path is bad".
set -euo pipefail
REL=$(readlink -f "$1"); OUT=$(readlink -f "$2"); MODE=${3:-}; ENTRY=${4:-chk}
B=$(dirname "$(readlink -f "$0")")
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
SYS=/usr/lib/syslinux/bios
MEMTEST=/boot/memtest86+/memtest.bin
XSESSION=${XSESSION:-/home/jantzen/xxri-build/rootfs/etc/skel/.xsession}

xorriso -osirrox on -indev "$REL" -extract / "$W/base" >/dev/null 2>&1
chmod -R u+w "$W/base"
for v in midmem:0x3fffffff lowmem:0x2fffffff; do
	mkdir -p "$W/base/boot/${v%%:*}"
	python3 - "$W/base/boot/vmlinuz" "$W/base/boot/${v%%:*}/vmlinuz" "${v##*:}" <<'EOF'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read())
assert d[0x202:0x206] == b'HdrS'
struct.pack_into('<I', d, 0x22c, int(sys.argv[3], 16))      # initrd_addr_max
open(sys.argv[2], 'wb').write(d)
EOF
done
[ -f "$MEMTEST" ] && cp "$MEMTEST" "$W/base/boot/memtest.bin"
dd if="$REL" bs=432 count=1 of="$W/mbr405.bin" 2>/dev/null   # release isohybrid MBR

CHK="rdinit=/xd/init retain_initrd log_buf_len=1M LD_LIBRARY_PATH=/xd loglevel=4 panic=0"
VERB="loglevel=7 panic=0 brd.rd_nr=1 log_buf_len=1M"
SHIP="quiet loglevel=3 vga=791 vt.global_cursor_default=0 consoleblank=0"
SER=""; [ "$MODE" = test ] && SER="console=tty0 console=ttyS0,115200"

syslinux_cfg() { # $1 title  $2 "full" for all entries
	if [ "$MODE" = test ]; then
		printf 'SERIAL 0 115200\nDEFAULT %s\nPROMPT 0\nTIMEOUT 10\n' "$ENTRY"
	else
		printf 'DEFAULT chk\nUI menu.c32\nPROMPT 0\nTIMEOUT 0\n'
	fi
	cat <<EOF
MENU TITLE $1
MENU MARGIN 2
MENU ROWS 8
MENU TABMSGROW 16
MENU HELPMSGROW 17
MENU HELPMSGENDROW -2
MENU TABMSG Choose with the arrow keys, press ENTER. Photograph the result.

LABEL chk
MENU LABEL 1  Check core.gz handoff - standard (core.gz near 2 GB)
TEXT HELP
The placement that failed. Reproduces the problem.
ENDTEXT
KERNEL /boot/vmlinuz
INITRD /boot/xdiag.gz,/boot/core.gz
APPEND $CHK vga=791 cde $SER

LABEL chkmid
MENU LABEL 2  Check core.gz handoff - core.gz near 1 GB
TEXT HELP
Different address, but the kernel still copies it out of high memory.
ENDTEXT
KERNEL /boot/midmem/vmlinuz
INITRD /boot/xdiag.gz,/boot/core.gz
APPEND $CHK vga=791 cde $SER

LABEL chklow
MENU LABEL 3  Check core.gz handoff - core.gz below 768 MB
TEXT HELP
Known good on this laptop. No relocation happens.
ENDTEXT
KERNEL /boot/lowmem/vmlinuz
INITRD /boot/xdiag.gz,/boot/core.gz
APPEND $CHK vga=791 cde $SER

LABEL chktext
MENU LABEL 4  Check core.gz handoff - standard, no VESA mode set
TEXT HELP
Same as entry 1 but without vga=791, so the video BIOS is never called.
ENDTEXT
KERNEL /boot/vmlinuz
INITRD /boot/xdiag.gz,/boot/core.gz
APPEND $CHK cde $SER
EOF
	[ "${2:-}" = full ] || return 0
	cat <<EOF

LABEL memtest
MENU LABEL 5  Memory test (memtest86+) - runs until you stop it
TEXT HELP
Tests the RAM itself. Let it run at least one full pass, then photograph.
Press Esc to leave.
ENDTEXT
KERNEL /boot/memtest.bin

LABEL xcollect
MENU LABEL 6  Graphics diagnostics (no display takeover)
TEXT HELP
Boots normally, lets the session try X, then prints a report on VT1.
Press Alt+F1 after about a minute.
ENDTEXT
KERNEL /boot/lowmem/vmlinuz
INITRD /boot/xdiag.gz,/boot/core.gz
APPEND $CHK xdcollect vga=791 cde $SER

LABEL xfix
MENU LABEL 7  Graphics FIX test (waits for KMS properly)
TEXT HELP
Same, with the corrected .xsession installed. Does the desktop appear?
ENDTEXT
KERNEL /boot/lowmem/vmlinuz
INITRD /boot/xdiag.gz,/boot/core.gz
APPEND $CHK xdfix vga=791 cde $SER

LABEL verbose
MENU LABEL 8  Verbose boot, no checker
KERNEL /boot/vmlinuz
INITRD /boot/core.gz
APPEND $VERB vga=791 cde $SER

LABEL shipped
MENU LABEL 9  Normal XXRI Lite 2.5 boot (unchanged)
KERNEL /boot/vmlinuz
INITRD /boot/core.gz
APPEND $SHIP cde $SER
EOF
}

mkxdiag() { # $1 root  $2 label
	python3 "$B/mkxdiag.py" "$1/boot/core.gz" "$B/xd-init.sh" "$1/boot/xdiag.gz" "$2" \
		"$B/xd-xcollect.sh" "$XSESSION" >/dev/null
}

build_syslinux() { # $1 name  $2 title  $3 label  $4 full|slim  $5 mbr  $6.. loader files
	local name=$1 title=$2 label=$3 full=$4 mbr=$5; shift 5
	local r="$W/$name"
	cp -a "$W/base" "$r"
	mkxdiag "$r" "$label"
	if [ $# -gt 0 ]; then rm -f "$r/boot/isolinux/"*.c32; cp "$@" "$r/boot/isolinux/"; fi
	syslinux_cfg "$title" "$full" > "$r/boot/isolinux/isolinux.cfg"
	xorriso -as mkisofs -o "$OUT/$name.iso" -V XXRI_LITE -isohybrid-mbr "$mbr" \
		-b boot/isolinux/isolinux.bin -c boot/isolinux/boot.cat \
		-no-emul-boot -boot-load-size 4 -boot-info-table -J -R "$r" >/dev/null 2>&1
	rm -rf "$r"
}

build_grub() {
	local name=XXRI-VK24MX-diag-C-grub r="$W/grub" idx=0
	cp -a "$W/base" "$r"; rm -rf "$r/boot/isolinux"
	mkxdiag "$r" "GRUB 2"
	mkdir -p "$r/boot/grub"
	case "$ENTRY" in chkmid) idx=1 ;; chklow) idx=2 ;; chktext) idx=3 ;; esac
	{
		if [ "$MODE" = test ]; then
			echo "serial --unit=0 --speed=115200"; echo "terminal_input serial console"
			echo "terminal_output serial console"; echo "set timeout=1"; echo "set default=$idx"
		else
			echo "set timeout=-1"; echo "set default=0"
		fi
		cat <<EOF
insmod all_video
insmod vbe
insmod gzio
insmod iso9660
menuentry "1  Check core.gz handoff - GRUB's own placement" {
	set gfxpayload=1024x768x16,1024x768
	linux /boot/vmlinuz $CHK cde $SER
	initrd /boot/xdiag.gz /boot/core.gz
}
menuentry "2  Check core.gz handoff - core.gz near 1 GB" {
	set gfxpayload=1024x768x16,1024x768
	linux /boot/midmem/vmlinuz $CHK cde $SER
	initrd /boot/xdiag.gz /boot/core.gz
}
menuentry "3  Check core.gz handoff - core.gz below 768 MB" {
	set gfxpayload=1024x768x16,1024x768
	linux /boot/lowmem/vmlinuz $CHK cde $SER
	initrd /boot/xdiag.gz /boot/core.gz
}
menuentry "4  Check core.gz handoff - no VESA mode set" {
	set gfxpayload=text
	linux /boot/vmlinuz $CHK cde $SER
	initrd /boot/xdiag.gz /boot/core.gz
}
EOF
	} > "$r/boot/grub/grub.cfg"
	grub-mkrescue -d /usr/lib/grub/i386-pc -o "$OUT/$name.iso" "$r" -- -volid XXRI_LITE >/dev/null 2>&1
	rm -rf "$r"
}

mkdir -p "$OUT"
build_syslinux XXRI-VK24MX-diag-A-isolinux405 "XXRI 2.5 VK24MX diag A - ISOLINUX 4.05 (as released)" \
	"ISOLINUX 4.05" full "$W/mbr405.bin"
build_syslinux XXRI-VK24MX-diag-B-isolinux6 "XXRI 2.5 VK24MX diag B - ISOLINUX 6" \
	"ISOLINUX 6" slim "$SYS/isohdpfx.bin" "$SYS/isolinux.bin" "$SYS/ldlinux.c32" "$SYS/menu.c32" \
	"$SYS/libutil.c32" "$SYS/libcom32.c32"
build_grub
ls -l "$OUT"/*.iso
