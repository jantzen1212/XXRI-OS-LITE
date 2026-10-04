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
#   * early CPU microcode: uncompressed cpio images, in front of core.gz on
#     every boot entry; the "safe graphics" (nomodeset) entry
#   * GPU and Wi-Fi/Bluetooth firmware extensions matching their reviewed
#     lists, nouveau present, the regulatory database, the Bluetooth stack
#   * ISO size creeping away from the ~285 MiB budget (the RC: 243 MiB; with
#     the 2015-era GPU firmware and CPU microcode: 261 MiB; with Wi-Fi and
#     Bluetooth firmware and BlueZ: ~278 MiB)
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
[ $sz -gt $((400 * 1048576)) ] && note "ISO is over 400 MiB (budget is ~285 MiB)"
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
# Early CPU microcode (rules/xxri-ucode.make): GRUB names the images through
# $xxri_ucode, so the loop above does not see them.  The kernel only takes an
# UNCOMPRESSED newc archive at the very start of the initrd.
if [ -s "$W/grub.cfg" ]; then
	uc=$(grep -o '/boot/[a-z]*-ucode\.img' "$W/grub.cfg" | sort -u)
	[ -n "$uc" ] || bad "grub.cfg loads no CPU microcode image"
	for p in $uc; do
		v=${p#/boot/}; v=${v%-ucode.img}
		case $v in intel) want=GenuineIntel.bin ;; amd) want=AuthenticAMD.bin ;; *) want=unknown ;; esac
		if ! grep -qx "$p" "$W/files"; then bad "microcode $p is NOT on the ISO"; continue; fi
		x "$p" "$W/$v-ucode.img"
		[ "$(head -c 6 "$W/$v-ucode.img")" = 070701 ] &&
			cpio -it --quiet < "$W/$v-ucode.img" 2>/dev/null | grep -qx "kernel/x86/microcode/$want" &&
			ok "microcode $p: uncompressed early cpio with $want, $(( $(stat -c %s "$W/$v-ucode.img") / 1024 )) KiB" ||
			bad "microcode $p is not an uncompressed early cpio with kernel/x86/microcode/$want"
		grep -qx "/boot/LICENSE.$v-ucode" "$W/files" && ok "microcode licence /boot/LICENSE.$v-ucode" ||
			bad "/boot/LICENSE.$v-ucode missing (the vendor licence must ship with the image)"
	done
	n=$(awk 'tolower($1) == "initrd"' "$W/grub.cfg" | wc -l)
	m=$(awk 'tolower($1) == "initrd" && $2 == "$xxri_ucode"' "$W/grub.cfg" | wc -l)
	[ "$n" -gt 0 ] && [ "$n" = "$m" ] && ok "microcode in front of core.gz on all $n boot entries" ||
		bad "only $m of $n initrd lines load the microcode first"
	grep -qE '^[[:space:]]*linux .* nomodeset' "$W/grub.cfg" && ok "safe graphics boot entry (nomodeset)" ||
		bad "no safe graphics (nomodeset) boot entry"
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
# Empty directories and device nodes of Tiny Core's base (2.5/2.6 lost them
# on the way through git; the PTXdist build keeps Tiny Core's own entries).
for d in ('proc', 'sys', 'dev/pts', 'home', 'mnt', 'run', 'dev/console', 'dev/null'):
    if d not in names:
        print("  FAIL  /%s is missing from core.gz" % d)
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

echo "-- binaries --"
# Every XXRI program and library must find its libraries and symbol versions
# in core.gz + the boot-time extensions, and XXRI's own i586 programs must not
# contain instructions an i586 lacks unless they choose them at run time (the
# static boot splash carries glibc's CPUID-dispatched string functions).
mkdir -p "$W/bin" && x /boot/core.gz "$W/bin/core.gz" && x /cde "$W/bin/cde" && chmod -R u+w "$W/bin"
XXRI_ELF="/usr/local/bin/xxri- /usr/local/sbin/xxri- /usr/local/bin/flwm /usr/local/lib/libfm
	/usr/local/lib/libmenu-cache /usr/local/lib/libexif /usr/local/lib/libGLdispatch
	/usr/local/lib/libOpenGL /usr/local/lib/libGLESv2 /usr/local/lib/libglapi /usr/local/lib/xxri-browser
	/usr/local/bin/Xfbdev /usr/local/lib/xorg"
if python3 "$HERE/tools/qa/elf-closure.py" "$W/bin/core.gz" "$W/bin/cde" --check $XXRI_ELF > "$W/closure" 2>&1; then
	ok "$(tail -1 "$W/closure")"
else
	sed 's/^/  FAIL  /' "$W/closure" | head -20
fi
mkdir -p "$W/bin/core" && ( cd "$W/bin/core" && zcat ../core.gz | cpio -idm --quiet 2>/dev/null )
# the X server extension is XXRI-built and i586 too: lay it into the same tree
[ -f "$W/bin/cde/optional/xxri-xlibre.tcz" ] &&
	unsquashfs -q -f -d "$W/bin/core" "$W/bin/cde/optional/xxri-xlibre.tcz" >/dev/null 2>&1
grep -qx 'xxri-xlibre.tcz' "$W/bin/cde/onboot.lst" && ok "X server: xxri-xlibre.tcz (XLibre) in onboot.lst" ||
	bad "xxri-xlibre.tcz is not in onboot.lst: no X server at boot"
for gone in Xvesa.tcz xorg-server.tcz; do
	grep -qx "$gone" "$W/bin/cde/onboot.lst" && bad "$gone is still in onboot.lst (replaced by xxri-xlibre.tcz)"
done
# GPU firmware (config/firmware/gpu.list): loaded at boot, before the udev
# trigger that loads the KMS drivers, and exactly the reviewed files
if [ -f "$W/bin/cde/optional/xxri-firmware-gpu.tcz" ]; then
	grep -qx 'xxri-firmware-gpu.tcz' "$W/bin/cde/onboot.lst" && ok "GPU firmware: xxri-firmware-gpu.tcz in onboot.lst" ||
		bad "xxri-firmware-gpu.tcz is not in onboot.lst: radeon/i915 start without their firmware"
	fw=$(unsquashfs -l "$W/bin/cde/optional/xxri-firmware-gpu.tcz" 2>/dev/null | sed -n 's|^squashfs-root/usr/local/lib/firmware/||p' | grep '\.bin$' | sort)
	want=$(unsquashfs -q -cat "$W/bin/cde/optional/xxri-firmware-gpu.tcz" usr/local/share/doc/xxri-firmware-gpu/gpu.list 2>/dev/null | grep -v '^#' | sort)
	[ -n "$fw" ] && [ "$fw" = "$want" ] &&
		ok "GPU firmware: $(echo "$fw" | wc -l) files as listed, $(( $(stat -c %s "$W/bin/cde/optional/xxri-firmware-gpu.tcz") / 1024 )) KiB" ||
		bad "GPU firmware: xxri-firmware-gpu.tcz does not match its gpu.list"
else
	bad "xxri-firmware-gpu.tcz missing: legacy Radeon GPUs run without acceleration (NI cards not at all)"
fi
# Firmware from linux-firmware (config/firmware/{intel,amd,nvidia,wireless}
# .list): every name a list gives must resolve inside its extension, through
# the links linux-firmware's WHENCE defines - the build's own resolver
for v in intel amd nvidia wireless; do
	kind="GPU firmware"; [ $v = wireless ] && kind="Wi-Fi/Bluetooth firmware"
	t="$W/bin/cde/optional/xxri-firmware-$v.tcz"
	[ -f "$t" ] || { bad "xxri-firmware-$v.tcz missing"; continue; }
	grep -qx "xxri-firmware-$v.tcz" "$W/bin/cde/onboot.lst" && ok "$kind: xxri-firmware-$v.tcz in onboot.lst" ||
		bad "xxri-firmware-$v.tcz is not in onboot.lst: its devices start without firmware"
	unsquashfs -q -cat "$t" "usr/local/share/doc/xxri-firmware-$v/$v.list" > "$W/fw-$v.list" 2>/dev/null
	unsquashfs -lls "$t" > "$W/fw-$v.lls" 2>/dev/null
	python3 - "$HERE/ptxdist/scripts/xxri-fw-select.py" "$W/fw-$v.lls" "$W/fw-$v.list" "$v" "$kind" <<'EOF'
import importlib.util, sys
spec = importlib.util.spec_from_file_location('fw', sys.argv[1])
fw = importlib.util.module_from_spec(spec); spec.loader.exec_module(fw)
pre = 'squashfs-root/usr/local/lib/firmware/'
files, links, size = set(), {}, 0
for line in open(sys.argv[2]):
    f = line.split(None, 5)
    if len(f) < 6 or not f[5].startswith(pre):
        continue
    path = f[5].rstrip('\n')[len(pre):]
    if f[0].startswith('l'):
        name, _, target = path.partition(' -> ')
        links[name] = target
    elif f[0].startswith('-'):
        files.add(path); size += int(f[2])
names = fw.read_lists([sys.argv[3]])
bad = []
for n in names:
    try:
        fw.resolve(n, files, links)
    except SystemExit:
        bad.append(n)
if not names:
    print('  FAIL  xxri-firmware-%s.tcz: no %s.list inside' % (sys.argv[4], sys.argv[4]))
elif bad:
    print('  FAIL  xxri-firmware-%s.tcz lacks %d listed names, e.g. %s' % (sys.argv[4], len(bad), bad[0]))
else:
    print('  ok    %s: xxri-firmware-%s.tcz resolves all %d listed names (%d files, %d links, %d KiB)'
          % (sys.argv[5], sys.argv[4], len(names), len(files), len(links), size // 1024))
EOF
done
# cfg80211 asks for regulatory.db like any firmware; without it every
# country is the restrictive world domain
t="$W/bin/cde/optional/xxri-firmware-wireless.tcz"
[ -f "$t" ] && unsquashfs -l "$t" 2>/dev/null | grep -qx 'squashfs-root/usr/local/lib/firmware/regulatory.db' &&
	ok "Wi-Fi: regulatory.db in xxri-firmware-wireless.tcz" ||
	bad "regulatory.db missing: Wi-Fi stays on the world regulatory domain"
# firmware linux-firmware does not carry (Tiny Core's packages), the radio
# tools and the Bluetooth stack: present, and loaded at boot
for t in firmware-ipw2100.tcz firmware-ipw2200.tcz firmware-zd1211.tcz firmware-openfwwf.tcz \
         iw.tcz rfkill.tcz bluetooth-6.12.11-tinycore.tcz bluez5.tcz; do
	[ -f "$W/bin/cde/optional/$t" ] && grep -qx "$t" "$W/bin/cde/onboot.lst" ||
		bad "$t missing or not in onboot.lst"
done
unsquashfs -l "$W/bin/cde/optional/bluez5.tcz" 2>/dev/null | grep -q '/lib/bluetooth/bluetoothd$' &&
	unsquashfs -l "$W/bin/cde/optional/bluetooth-6.12.11-tinycore.tcz" 2>/dev/null | grep -q '/btusb\.ko' &&
	ok "Bluetooth: bluez5 (bluetoothd) and the kernel's btusb.ko are on the medium" ||
	bad "Bluetooth: bluetoothd or btusb.ko missing"
t=nouveau-6.12.11-tinycore.tcz
[ -f "$W/bin/cde/optional/$t" ] && grep -qx "$t" "$W/bin/cde/onboot.lst" &&
	unsquashfs -l "$W/bin/cde/optional/$t" 2>/dev/null | grep -q '/nouveau\.ko' &&
	ok "NVIDIA: $t (nouveau.ko) in onboot.lst" || bad "$t missing or not in onboot.lst: NVIDIA GPUs get no KMS driver"
# both servers need root (no logind): a lost setuid bit only shows as "no
# desktop" on real hardware, so check the modes in the extension itself
if [ -f "$W/bin/cde/optional/xxri-xlibre.tcz" ]; then
	modes=$(unsquashfs -ll "$W/bin/cde/optional/xxri-xlibre.tcz" 2>/dev/null)
	for f in usr/local/lib/xorg/Xorg.wrap usr/local/bin/Xfbdev; do
		echo "$modes" | grep -q "^-rwsr-xr-x root/root .* squashfs-root/$f\$" && ok "X server: /$f setuid root" ||
			bad "X server: /$f is not setuid root - X cannot start without logind"
	done
	grep -qx 'needs_root_rights=yes' "$W/bin/core/usr/local/etc/X11/Xwrapper.config" 2>/dev/null &&
		grep -qx 'allowed_users=anybody' "$W/bin/core/usr/local/etc/X11/Xwrapper.config" &&
		ok "X server: Xwrapper.config lets the autologin session start Xorg as root" ||
		bad "X server: /usr/local/etc/X11/Xwrapper.config missing or restrictive - Xorg.wrap refuses the session"
fi
python3 "$HERE/tools/qa/isa-scan.py" $(cd "$W/bin/core" && for p in $XXRI_ELF; do ls -d ."$p"* 2>/dev/null; done |
	sed "s|^\.|$W/bin/core|") 2>/dev/null | awk -F'\t' 'NR > 1 {
		above = $3 + $4 + $5 + $6 + $7 + $8
		if (above && !$9) { printf "  FAIL  %s: %d P6/MMX/SSE instructions without a CPUID check (i586 baseline)\n", $2, above; bad++ }
		else if (above) printf "  info  %s: %d above-i586 instructions behind %d CPUID checks\n", $2, above, $9 }
	END { if (!bad) print "  ok    XXRI programs (core.gz, X server): nothing above i586 outside CPUID dispatch" }' |
	sed "s|$W/bin/core||"

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
