#!/bin/bash
# validate-release.sh - pre-release gate for an XXRI Lite ISO.
#
#   ./validate-release.sh output/XXRI-Lite-2.6.iso [--boot]
#
# Checks the things that have actually bitten us, plus the 2.6 boot contract:
#   * core.gz that is truncated, corrupt, or missing /init  -> the VK24MX class
#     of "Kernel panic - VFS: Unable to mount root fs" with no explanation
#   * boot loader entries pointing at files that are not on the ISO
#   * the unified boot layout: El Torito BIOS + El Torito UEFI, hybrid MBR,
#     an MBR partition of type 0xEF holding EFI/BOOT/BOOTIA32.EFI, the same
#     loader in the ISO tree (for Rufus-style file copies), one GRUB version
#     across every core image and module directory, the marker file the
#     loaders search for, and the GPL licence/source notice for GRUB
#   * the installer's boot tooling (xxri-grub.tcz and friends) and the absence
#     of EXTLINUX, whose high initrd placement 2.6 exists to remove
#   * build junk swept into core.gz (rootfs/.kilo/worktrees is a nested
#     checkout that once added 56 MB)
#   * ISO size creeping away from the ~268 MiB budget
# With --boot it boots the ISO through its REAL firmware and loader in QEMU -
# SeaBIOS and 32-bit OVMF, each as CD, hybrid disk and USB stick - and
# requires the guest's own userspace (its DHCP request) and a lit desktop;
# checks that 64-bit OVMF refuses the medium cleanly (documented limitation);
# and boots the extracted kernel + core.gz directly at 1 and 16 GiB with a
# serial console, which a truncated or corrupt core.gz cannot survive.
set -uo pipefail
ISO=${1:?usage: validate-release.sh ISO [--boot]}
BOOT=${2:-}
HERE=$(cd "$(dirname "$0")" && pwd)
BT="$HERE/tools/qemu-boot-test.py"
OVMF32=${XXRI_OVMF32:-/home/jantzen/.xxri-testenv/fw/deb/usr/share/OVMF}
W=$(mktemp -d); LOG=$(mktemp); trap 'rm -rf "$W" "$LOG"' EXIT
ok()   { printf '  ok    %s\n' "$*"; }
bad()  { printf '  FAIL  %s\n' "$*"; }
note() { printf '  warn  %s\n' "$*"; }
x()    { xorriso -osirrox on -indev "$ISO" -extract "$1" "$2" >/dev/null 2>&1; }

# Everything runs inside main() so failures reported by the embedded python
# checks are counted too: the summary greps the transcript rather than trying
# to carry counters across subshells.
main() {
echo "== $ISO =="
[ -f "$ISO" ] || { bad "no such file"; exit 1; }
sz=$(stat -c%s "$ISO")
echo "  size  $((sz / 1048576)) MiB"
[ $sz -gt $((400 * 1048576)) ] && note "ISO is over 400 MiB (budget is ~268 MiB)"
[ -f "$ISO.sha256" ] && ( cd "$(dirname "$ISO")" && sha256sum -c --quiet "$(basename "$ISO").sha256" ) \
	&& ok "sha256 matches $(basename "$ISO").sha256"

echo "-- boot record --"
xorriso -indev "$ISO" -report_system_area plain -report_el_torito plain 2>/dev/null > "$W/sysarea"
grep -q "grub2-mbr" "$W/sysarea" \
	&& ok "hybrid MBR with GRUB boot code (dd / Etcher / Rufus DD mode)" \
	|| bad "no GRUB hybrid MBR: this ISO will not boot when written to a USB stick"
grep -qE "El Torito boot img :.*BIOS" "$W/sysarea" && ok "El Torito BIOS boot image" \
	|| bad "no El Torito BIOS boot image"
grep -qE "El Torito boot img :.*UEFI" "$W/sysarea" && ok "El Torito UEFI boot image" \
	|| bad "no El Torito UEFI boot image"
grep -qE "MBR partition +: +[0-9]+ +0x80 " "$W/sysarea" && ok "an MBR partition is marked active" \
	|| bad "no active MBR partition (some BIOSes refuse to boot such a stick)"
esp=$(awk '/MBR partition +: +[0-9]/ && $6 == "0xef" { print $7, $8 }' "$W/sysarea" | head -1)
if [ -n "$esp" ]; then
	set -- $esp
	ok "MBR partition type 0xEF (EFI system partition) at sector $1, $2 sectors"
	dd if="$ISO" of="$W/esp.img" bs=512 skip="$1" count="$2" status=none
	if MTOOLS_SKIP_CHECK=1 mcopy -n -i "$W/esp.img" ::/EFI/BOOT/BOOTIA32.EFI "$W/esp-ia32.efi" 2>/dev/null; then
		m=$(python3 -c "import struct,sys; d=open(sys.argv[1],'rb').read(); o=struct.unpack_from('<I',d,0x3c)[0]; print(hex(struct.unpack_from('<H',d,o+4)[0]))" "$W/esp-ia32.efi")
		[ "$m" = 0x14c ] && ok "ESP holds EFI/BOOT/BOOTIA32.EFI (PE32, machine i386)" \
			|| bad "ESP BOOTIA32.EFI has machine type $m, not i386 (0x14c)"
	else
		bad "ESP has no EFI/BOOT/BOOTIA32.EFI: 32-bit UEFI firmware will not boot this medium"
	fi
	MTOOLS_SKIP_CHECK=1 mdir -b -i "$W/esp.img" ::/EFI/BOOT/ 2>/dev/null | grep -qi 'BOOTX64.EFI' \
		&& note "ESP carries BOOTX64.EFI (the experimental x86_64 loader; see docs/boot-2.6.md)"
else
	bad "no MBR partition of type 0xEF: UEFI firmware will not boot this medium from USB"
fi

echo "-- files --"
xorriso -indev "$ISO" -find / -type f 2>/dev/null | tr -d "'" | sed 's/^ *//' > "$W/files"
x /boot/grub/grub.cfg "$W/grub.cfg"
x /EFI/BOOT/BOOTIA32.EFI "$W/tree-ia32.efi"
if [ -s "$W/tree-ia32.efi" ] && [ -s "$W/esp-ia32.efi" ]; then
	cmp -s "$W/tree-ia32.efi" "$W/esp-ia32.efi" && ok "/EFI/BOOT/BOOTIA32.EFI in the ISO tree == ESP copy (Rufus ISO mode)" \
		|| bad "/EFI/BOOT/BOOTIA32.EFI in the tree differs from the ESP copy"
else
	bad "no /EFI/BOOT/BOOTIA32.EFI in the ISO tree: file-copy writers (Rufus ISO mode) lose UEFI boot"
fi
if [ -s "$W/grub.cfg" ]; then
	ok "boot loader config: GRUB (/boot/grub/grub.cfg)"
	{ awk 'tolower($1) == "linux" { print $2 }' "$W/grub.cfg"
	  awk 'tolower($1) == "initrd" { for (i = 2; i <= NF; i++) print $i }' "$W/grub.cfg"
	} | grep '^/' | sort -u | while read -r p; do
		grep -qx "$p" "$W/files" && ok "referenced $p exists" || bad "referenced $p is NOT on the ISO"
	done
	grep -qE '(^|[[:space:]])(mem|memmap|initrd_addr|highmem)=' "$W/grub.cfg" \
		&& bad "grub.cfg imposes a memory limit (mem=/memmap=/...): XXRI adds no memory ceilings" \
		|| ok "no memory ceiling on any kernel command line"
else
	bad "no /boot/grub/grub.cfg"
fi
mark=$(grep -x '/\.disk/xxri-lite-.*' "$W/files" | head -1)
[ -n "$mark" ] && ok "loader marker file $mark" || bad "no /.disk/xxri-lite-* marker: the core images cannot find this medium"
for f in /boot/grub/COPYING /boot/grub/SOURCE.txt; do
	grep -qx "$f" "$W/files" && ok "GPL notice $f" || bad "$f missing (GRUB is GPLv3: licence + source offer must ship)"
done
[ -d "$(dirname "$ISO")/sources" ] && ls "$(dirname "$ISO")"/sources/grub-*.tar.xz >/dev/null 2>&1 \
	&& ok "GRUB Corresponding Source next to the ISO ($(ls "$(dirname "$ISO")"/sources/grub-*.tar.xz | xargs -n1 basename))" \
	|| note "no sources/grub-*.tar.xz next to the ISO: publish it with the release"

echo "-- GRUB --"
x /boot/grub/i386-pc/eltorito.img "$W/eltorito.img"
x /boot/grub/i386-pc/normal.mod "$W/pc-normal.mod"
x /boot/grub/i386-efi/normal.mod "$W/efi-normal.mod"
vers=""
for f in eltorito.img tree-ia32.efi pc-normal.mod efi-normal.mod; do
	# GRUB stores PACKAGE_VERSION in its core and in normal.mod; eltorito.img's
	# core is LZMA-compressed, so it is checked through the matching modules.
	[ -s "$W/$f" ] || { bad "$f missing"; continue; }
	v=$(strings -n 3 "$W/$f" | grep -m1 -oE '^2\.[0-9]+(\.[0-9]+)?$' || true)
	[ -n "$v" ] && vers="$vers $v"
done
uv=$(echo $vers | tr ' ' '\n' | sort -u)
[ "$(echo "$uv" | grep -c .)" = 1 ] && ok "one GRUB version everywhere: $uv" \
	|| bad "mixed or unreadable GRUB versions: [$vers] - core images and modules must come from one build"
for p in i386-pc i386-efi; do
	n=$(grep -c "^/boot/grub/$p/.*\.mod$" "$W/files")
	[ "$n" -gt 250 ] && ok "$p modules: $n" || bad "$p module directory incomplete ($n modules)"
done

echo "-- installer boot tooling --"
for t in xxri-grub gdisk dosfstools efibootmgr efivar popt; do
	if grep -qx "/cde/optional/$t.tcz" "$W/files"; then
		x "/cde/optional/$t.tcz" "$W/$t.tcz"; x "/cde/optional/$t.tcz.md5.txt" "$W/$t.md5"
		[ "$(md5sum < "$W/$t.tcz" | cut -c1-32)" = "$(cut -c1-32 "$W/$t.md5" 2>/dev/null)" ] \
			&& ok "$t.tcz present, md5 ok" || bad "$t.tcz md5 mismatch"
	else
		bad "/cde/optional/$t.tcz missing: the installer cannot put GRUB on a disk"
	fi
done
[ -s "$W/xxri-grub.tcz" ] && unsquashfs -l "$W/xxri-grub.tcz" 2>/dev/null | grep -q 'sbin/grub-install$' \
	&& ok "xxri-grub.tcz carries grub-install" || bad "xxri-grub.tcz has no grub-install"

echo "-- kernel --"
x /boot/vmlinuz "$W/vmlinuz"
if [ -s "$W/vmlinuz" ]; then
	python3 - "$W/vmlinuz" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
if d[0x202:0x206] != b'HdrS':
    print("  FAIL  vmlinuz has no HdrS magic"); sys.exit(1)
v = struct.unpack_from('<H', d, 0x206)[0]
m = struct.unpack_from('<I', d, 0x22c)[0]
xl = struct.unpack_from('<H', d, 0x236)[0]
print("  ok    boot protocol %d.%02d, initrd_addr_max 0x%08x (%d MiB, unpatched)" % (v >> 8, v & 0xff, m, m // 1048576))
if m != 0x7fffffff:
    print("  FAIL  initrd_addr_max was modified: XXRI ships the stock kernel")
print("  %s  EFI stub: %s, xloadflags 0x%04x (64-bit kernel: %s, EFI handover 32: %s, 64: %s)" % (
    'ok   ' if d[:2] == b'MZ' else 'FAIL ', 'yes' if d[:2] == b'MZ' else 'NO', xl,
    'yes' if xl & 1 else 'no', 'yes' if xl & 4 else 'no', 'yes' if xl & 8 else 'no'))
EOF
else bad "no /boot/vmlinuz on the ISO"; fi

echo "-- core.gz --"
x /boot/core.gz "$W/core.gz"
if [ ! -s "$W/core.gz" ]; then
	bad "no /boot/core.gz on the ISO"
else
	gzip -t "$W/core.gz" 2>/dev/null && ok "gzip stream intact" || bad "gzip stream is corrupt or truncated"
	python3 - "$W/core.gz" <<'EOF' || true
import stat, sys, zlib
raw = zlib.decompressobj(31).decompress(open(sys.argv[1], 'rb').read())
names, off, n = {}, 0, 0
while off < len(raw):
    h = raw[off:off + 110]
    if h[:6] != b'070701':
        print("  FAIL  cpio stream broken at byte %d" % off); sys.exit(1)
    f = lambda a, b: int(h[a:b], 16)
    mode, fsize, nsize = f(14, 22), f(54, 62), f(94, 102)
    name = raw[off + 110:off + 110 + nsize - 1].decode('latin1')
    doff = (off + 110 + nsize + 3) & ~3
    if name == 'TRAILER!!!':
        print("  ok    cpio archive complete: %d entries, %.1f MiB unpacked" % (n, off / 1048576))
        break
    names[name.lstrip('./')] = (mode, fsize, doff)
    off = (doff + fsize + 3) & ~3
    n += 1
else:
    print("  FAIL  cpio archive has no TRAILER (truncated)"); sys.exit(1)

for must in ('init', 'bin/busybox', 'sbin/init', 'etc/skel/.xsession', 'etc/init.d/tc-config'):
    if must in names:
        print("  ok    contains /%s" % must)
    else:
        print("  FAIL  /%s is MISSING - the kernel would panic with 'unable to mount root fs'" % must)
# What git cannot store and a clean checkout used to lose (rootfs.meta).
for d in ('proc', 'sys', 'dev/pts', 'home', 'mnt', 'run'):
    if d not in names:
        print("  FAIL  /%s is missing (empty directory lost - was rootfs.meta applied?)" % d)
for p, bit, what in (('bin/busybox.suid', stat.S_ISUID, 'setuid'), ('usr/bin/sudo', stat.S_ISUID, 'setuid'),
                     ('tmp', stat.S_ISVTX, 'sticky')):
    if p in names and not names[p][0] & bit:
        print("  FAIL  /%s lost its %s bit" % (p, what))
if 'usr/local/sbin/extlinux' in names:
    print("  FAIL  EXTLINUX still ships: its high initrd placement is the path 2.6 removes")
else:
    print("  ok    no EXTLINUX in the system (installer uses GRUB)")
pos = names['init'][2] / len(raw) * 100 if 'init' in names else 0
print("  info  /init sits at %.0f%% of the archive (damage before that point = VFS panic)" % pos)
for junk in ('.kilo', '.git', 'worktrees'):
    hits = [k for k in names if k.split('/')[0] == junk or ('/%s/' % junk) in k]
    if hits:
        print("  warn  %d build-junk entries under %r shipped in core.gz (e.g. %s)" % (len(hits), junk, hits[0]))
big = sorted(((v[1], k) for k, v in names.items()), reverse=True)[:3]
print("  info  largest files: " + ", ".join("%s (%.1f MiB)" % (k, s / 1048576) for s, k in big))
EOF
fi

if [ "$BOOT" = --boot ]; then
	echo "-- QEMU boot through the real firmware + GRUB --"
	if ! command -v qemu-system-i386 >/dev/null || [ ! -x "$BT" ]; then
		note "qemu-system-i386 or tools/qemu-boot-test.py missing; boot checks skipped"
	else
		FWS="bios"
		[ -f "$OVMF32/OVMF32_CODE_4M.fd" ] && FWS="bios ia32" ||
			note "no 32-bit OVMF at $OVMF32 (set XXRI_OVMF32): IA32 UEFI boots skipped"
		for fw in $FWS; do for media in cd hdd usb; do
			( python3 "$BT" --fw $fw --media $media --mem 2048 --img "$ISO" --shots 90 --timeout 120 \
				--out "$W/boot-$fw-$media" >/dev/null 2>&1 ) &
		done; done
		( python3 "$BT" --fw x64 --media usb --mem 2048 --img "$ISO" --shots 45 --timeout 50 \
			--out "$W/boot-x64-usb" >/dev/null 2>&1 ) &
		wait
		for fw in $FWS; do for media in cd hdd usb; do
			python3 - "$W/boot-$fw-$media/result.json" "$fw" "$media" <<'EOF'
import json, sys
try:
    r = json.load(open(sys.argv[1]))
except Exception:
    print("  FAIL  %s %s: no result" % tuple(sys.argv[2:])); sys.exit()
b = r['shots'][0]['brightness'] if r['shots'] else -1
tag = "%s/%s" % (sys.argv[2], sys.argv[3])
if r['grub_menu'] is not None and r['userspace'] is not None and b >= 30:
    print("  ok    %-8s GRUB menu %.1fs, userspace %.1fs, desktop lit (%s)" % (tag, r['grub_menu'], r['userspace'], b))
else:
    print("  FAIL  %-8s GRUB menu %s, userspace %s, screen %s" % (tag, r['grub_menu'], r['userspace'], b))
EOF
		done; done
		if grep -aq "No bootable option or device was found" "$W/boot-x64-usb/serial.log" 2>/dev/null; then
			ok "x64 UEFI: firmware reports no bootable option (expected: 32-bit UEFI loader only)"
		elif grep -aq "GNU GRUB" "$W/boot-x64-usb/serial.log" 2>/dev/null; then
			note "x64 UEFI: GRUB started - this ISO carries the experimental BOOTX64.EFI"
		else
			note "x64 UEFI: no firmware verdict captured"
		fi
		# Direct kernel + core.gz with a serial console: the integrity check a
		# truncated or corrupt core.gz cannot survive (not a boot-path test).
		for mem in 1024 16384; do
			log="$W/direct-$mem.log"
			timeout 180 qemu-system-i386 -m $mem -snapshot -kernel "$W/vmlinuz" \
				-initrd "$W/core.gz" -append "loglevel=6 console=ttyS0,115200 cde" \
				-nographic -serial file:"$log" -display none >/dev/null 2>&1
			if grep -aq "init started: BusyBox" "$log" 2>/dev/null; then
				ok "direct kernel + core.gz reach userspace at ${mem} MiB RAM"
			else
				bad "direct kernel + core.gz did NOT reach userspace at ${mem} MiB RAM"
				grep -aE "Kernel panic|RAMDISK|not initramfs|Please append" "$log" 2>/dev/null |
					tail -3 | sed 's/^/        /'
			fi
		done
	fi
fi

}

main 2>&1 | tee "$LOG"
fail=$(grep -c '^  FAIL' "$LOG" || true)
warn=$(grep -c '^  warn' "$LOG" || true)
echo
echo "== $fail failure(s), $warn warning(s) =="
[ "$fail" -eq 0 ]
