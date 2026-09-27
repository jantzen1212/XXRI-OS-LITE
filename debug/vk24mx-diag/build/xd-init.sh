#!/xd/busybox sh
# XXRI 2.5 initrd handoff check (NEC VersaPro VK24MX investigation).
# Loaded as the FIRST initrd, ahead of core.gz, and run via rdinit=/xd/init
# with retain_initrd, so /sys/firmware/initrd holds the exact bytes the
# kernel received.  Everything it needs lives under /xd, which core.gz never
# writes, so it still runs when core.gz is damaged.
#
# Modes (boot code):
#   (none)      check the handoff, then continue the normal boot
#   xdcollect   check, then collect graphics diagnostics after the session starts
#   xdfix       as xdcollect, but install the fixed .xsession first
PATH=/xd; export PATH
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
. /xd/expected.sh
MODE=check
grep -qw xdcollect /proc/cmdline && MODE=collect
grep -qw xdfix /proc/cmdline && MODE=fix
R=/xd/report.txt; : > $R
say() { echo "$*"; echo "$*" >> $R; }
klog() { dmesg | sed 's/^\[[ 0-9.]*\] //'; }
clear
say "=== XXRI 2.5 initrd handoff check [$LABEL] kernel $(uname -r) mode=$MODE ==="
say "cmdline: $(cut -c1-180 /proc/cmdline)"

say "--- kernel log: initrd / memory ---"
klog | grep -E 'RAMDISK|INITRD|initrd|rootfs image|nitramfs|only 4GB|HIGHMEM avail' \
     | grep -v 'command line' | while read -r l; do say "  $l"; done
say "--- BIOS-e820 map ---"
klog | grep 'BIOS-e820:' | sed 's/BIOS-e820: //' | while read -r l; do say "  $l"; done

# ash arithmetic is 32-bit here; awk parses 64-bit hex addresses correctly.
set -- $(klog | sed -n 's/^RAMDISK: \[mem \(0x[0-9a-f]*\)-\(0x[0-9a-f]*\)\].*/\1 \2/p' | head -n 1)
PHYS=$1
if [ -n "$1" ]; then
	inside=$(klog | sed -n 's/.*BIOS-e820: \[mem \(0x[0-9a-f]*\)-\(0x[0-9a-f]*\)\] usable.*/\1 \2/p' |
		awk -v s="$1" -v e="$2" '{ if (s + 0 >= $1 + 0 && e + 0 <= $2 + 0) f = 1 } END { print f ? "yes" : "NO" }')
	say "  loader placed the initrd at $1-$2; inside one e820 'usable' range: $inside"
	if klog | grep -q '^Move RAMDISK'; then
		say "  the kernel RELOCATED it into low memory (high placement)"
	else
		say "  the kernel used it in place (no relocation)"
	fi
fi

IRD=/sys/firmware/initrd
say "--- bytes the kernel received ---"
if [ ! -r $IRD ]; then
	say "  /sys/firmware/initrd missing: kernel kept no initrd copy"
	say "==> PLEASE PHOTOGRAPH THIS SCREEN.  Shell follows ('reboot -f' to restart)."
	exec /xd/sh
fi
GOT=$(wc -c < $IRD)
CGOT=$((GOT - CORE_OFF))
say "  total $GOT bytes = checker $CORE_OFF + core.gz part $CGOT (core.gz on the ISO: $CORE_LEN)"
H=$(dd if=$IRD bs=2048 skip=$((CORE_OFF / 2048)) count=1 2>/dev/null | head -c 16 | md5sum | cut -c1-32)
[ "$H" = "$HEAD16" ] || say "  WARNING: core.gz does not start where expected (offset $CORE_OFF)"

printf "  comparing $CHUNKS blocks of 64 KB "
nb=0; bads=""; n=0
while read -r sum; do
	if [ $n -lt $((CHUNKS - 1)) ]; then
		got=$(dd if=$IRD bs=2048 skip=$((CORE_OFF / 2048 + n * 32)) count=32 2>/dev/null | md5sum | cut -c1-32)
	else
		got=$(dd if=$IRD bs=2048 skip=$((CORE_OFF / 2048 + n * 32)) count=32 2>/dev/null | head -c $LASTLEN | md5sum | cut -c1-32)
	fi
	[ "$got" = "$sum" ] || { nb=$((nb + 1)); bads="$bads $n"; }
	n=$((n + 1)); [ $((n % 16)) -eq 0 ] && printf "."
done < /xd/c64.md5
echo

: > /xd/bad.txt
drilled=0
for c in $bads; do
	[ $drilled -ge 24 ] && break
	drilled=$((drilled + 1)); s=0
	while [ $s -lt 32 ]; do
		sec=$((c * 32 + s)); [ $sec -ge $SECTORS ] && break
		len=2048; [ $sec -eq $((SECTORS - 1)) ] && len=$LASTSEC
		got=$(dd if=$IRD bs=2048 skip=$((CORE_OFF / 2048 + sec)) count=1 2>/dev/null | head -c $len | md5sum | cut -c1-32)
		exp=$(sed -n "$((sec + 1))p" /xd/s2k.md5)
		if [ "$got" != "$exp" ]; then
			if [ "$got" = "$Z2K" ]; then k=zeros
			elif [ "$got" = "$F2K" ]; then k=0xFF
			elif [ "$got" = "$E0" ]; then k=missing
			else
				o=$(grep -n -m 1 "^$got\$" /xd/s2k.md5 | cut -d: -f1)
				if [ -n "$o" ]; then k="shift$((o - 1 - sec))"; else k=garbage; fi
			fi
			echo "$sec $k" >> /xd/bad.txt
		fi
		s=$((s + 1))
	done
done

last=0
while read -r pct path; do [ -e "/$path" ] && last=$pct; done < /xd/markers.txt
full=no; [ -e /init ] && [ -e /bin/busybox ] && full=yes
err=$(klog | sed -n 's/.*rootfs image is not initramfs (\(.*\)).*/\1/p' | head -n 1)

say "--- RESULT ---"
[ $CGOT -lt $CORE_LEN ] && say "  TRUNCATED: the kernel got $CGOT of $CORE_LEN bytes of core.gz"
if [ $nb -gt 0 ]; then
	say "  CORRUPTED: $nb of $CHUNKS 64 KB blocks differ from the ISO's core.gz"
	say "  bad 2 KB sectors, with the PHYSICAL address the loader put them at:"
	awk '{ if (NR > 1 && $1 == p + 1 && $2 == k) { p = $1; next }
	       if (NR > 1) print s, p, k; s = $1; p = $1; k = $2 }
	     END { if (NR) print s, p, k }' /xd/bad.txt | head -n 12 | while read -r a b k; do
		case $k in shift*) k="data from ${k#shift} sectors away" ;; esac
		pa=$(awk -v base="$PHYS" -v off="$CORE_OFF" -v s="$a" 'BEGIN { printf "0x%x", base + 0 + off + s * 2048 }')
		pb=$(awk -v base="$PHYS" -v off="$CORE_OFF" -v s="$b" 'BEGIN { printf "0x%x", base + 0 + off + s * 2048 + 2047 }')
		say "    sectors $a-$b = phys $pa-$pb: $k"
	done
	say "  total bad sectors found: $(wc -l < /xd/bad.txt)"
else
	say "  core.gz bytes are IDENTICAL to the ISO's core.gz (all $CHUNKS blocks match)"
fi
say "  unpack: complete=$full, files present up to ~$last% of the archive, kernel error: ${err:-none}"

finish_boot() {
	umount /sys 2>/dev/null; umount /proc 2>/dev/null
	unset LD_LIBRARY_PATH; PATH=/sbin:/bin:/usr/sbin:/usr/bin; export PATH
	exec /init
}

if [ "$MODE" != check ]; then
	# Graphics diagnostics: collect AFTER the session has had its go, and print
	# to tty1 only - never take the display away from the tester.
	if [ "$MODE" = fix ] && [ -f /xd/xsession.fixed ]; then
		cp /xd/xsession.fixed /etc/skel/.xsession 2>/dev/null
		for h in /home/*; do [ -f "$h/.xsession" ] && cp /xd/xsession.fixed "$h/.xsession"; done
		chmod 755 /etc/skel/.xsession 2>/dev/null
		say "  installed the fixed .xsession (KMS readiness wait)"
	fi
	[ -f /opt/bootlocal.sh ] || : > /opt/bootlocal.sh
	echo '( sleep 45; /xd/xcollect.sh ) &' >> /opt/bootlocal.sh
	chmod 755 /opt/bootlocal.sh /xd/xcollect.sh 2>/dev/null
	say "==> continuing the boot; graphics report lands on VT1 (Alt+F1) in ~45 s"
	sleep 4
	finish_boot
fi

if [ $nb -eq 0 ] && [ $CGOT -ge $CORE_LEN ] && [ $full = yes ]; then
	say "==> HANDOFF OK. Normal boot continues in 30 s - press ENTER to stay here instead."
	if read -t 30 x; then exec /xd/sh; fi
	finish_boot
fi
say "==> PLEASE PHOTOGRAPH THIS SCREEN.  Shell follows ('more /xd/report.txt', 'reboot -f')."
exec /xd/sh
