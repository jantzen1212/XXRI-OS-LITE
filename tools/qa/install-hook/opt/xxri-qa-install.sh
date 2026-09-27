#!/bin/sh
# QA ONLY - never shipped.  Appended as a second initrd from GRUB's editor by
# tools/qa/install-test.sh.  Runs xxri-install-backend unattended in the live
# session, copies its log to a 2 MiB raw "log disk", then powers off.
#   xxriqa=auto          whole-disk automatic layout on the 6 GiB target disk
#   xxriqa=manual:N      install into partition N of the (pre-made) target disk
sleep 8
MODE=$(sed -n 's/.*xxriqa=\([^ ]*\).*/\1/p' /proc/cmdline)
T=""; L=""
for b in /sys/block/sd? /sys/block/hd?; do
	[ -e "$b/size" ] || continue
	s=$(cat "$b/size")
	[ "$s" = 12582912 ] && T=${b##*/}
	[ "$s" = 4096 ] && L=${b##*/}
done
LOG=/tmp/qa.log
{
	echo "QA mode=$MODE target=$T log=$L efi=$(cat /sys/firmware/efi/fw_platform_size 2>/dev/null || echo none)"
	case "$MODE" in
		auto) printf 'MODE=auto\nDISK=/dev/%s\n' "$T" ;;
		manual:*) printf 'MODE=manual\nPART=/dev/%s%s\n' "$T" "${MODE#manual:}" ;;
	esac > /tmp/qa.cfg
	printf 'USERNAME=xxri\nPASSWORD=\nHOSTNAME=xxri-qa\nAUTOLOGIN=1\nTZSTR=UTC\nLANGUAGE=en_US\nKEYMAP=us\n' >> /tmp/qa.cfg
	cat /tmp/qa.cfg
	/usr/sbin/xxri-install-backend run /tmp/qa.cfg
	echo "BACKEND_RC=$?"
	echo "--- fdisk"; fdisk -l "/dev/$T"
	echo "--- sgdisk"; sgdisk -p "/dev/$T"
	echo "--- grub logs"; cat /tmp/xxri-grub-pc.log /tmp/xxri-grub-efi.log 2>/dev/null | tail -20
	echo "QA_END"
} > $LOG 2>&1
dd if=$LOG of=/dev/$L bs=512 conv=sync 2>/dev/null
sync
poweroff
