#!/bin/sh
# Graphics diagnostics for the VK24MX investigation.  Runs from bootlocal.sh
# AFTER the session has already tried to start X, so it captures that attempt
# instead of making a new one.  It never starts an X server and never touches
# the display: output goes to a file and to VT1.
OUT=/tmp/xxri-xdiag.txt
{
	echo "=== XXRI graphics diagnostics  $(date 2>/dev/null) ==="
	echo "kernel : $(uname -r)"
	echo "cmdline: $(cat /proc/cmdline)"
	echo
	echo "--- which X server the session ended up with ---"
	ps 2>/dev/null | grep -E '[X]org|[X]vesa' || echo "  no X server running"
	echo
	echo "--- device nodes ---"
	ls -l /dev/dri/ 2>&1
	ls -l /dev/fb* 2>&1
	echo
	echo "--- sysfs: is the DRM device fully registered? ---"
	ls /sys/class/drm/ 2>&1
	for s in /sys/class/drm/card0-*/status; do
		[ -e "$s" ] && echo "  $s = $(cat "$s" 2>&1)"
	done
	echo "  driver bound: $(readlink /sys/class/drm/card0/device/driver 2>&1)"
	echo
	echo "--- can the nodes be opened (as root, now)? ---"
	for d in /dev/dri/card0 /dev/fb0; do
		if ( : < "$d" ) 2>/dev/null; then
			echo "  $d: OPEN OK"
		else
			echo "  $d: OPEN FAILED -> $( ( : < "$d" ) 2>&1 | sed 's/.*: //')"
		fi
	done
	echo
	echo "--- modules ---"
	lsmod 2>/dev/null | grep -E 'i915|drm|video|fb' || echo "  (none)"
	echo
	echo "--- kernel log: graphics ---"
	dmesg | grep -iE 'i915|drm|fb0|vesafb|firmware|framebuffer' | tail -n 40
	echo
	echo "--- X.Org drivers present ---"
	ls /usr/local/lib/xorg/modules/drivers/ 2>&1
	echo
	echo "--- /tmp/xorg.log (tail) ---"
	tail -n 60 /tmp/xorg.log 2>&1
} > $OUT 2>&1

# Short summary on VT1; the full file stays for photographing with `more`.
{
	echo
	echo "=============== XXRI graphics diagnostics ==============="
	echo "full report: $OUT   (view with:  more $OUT )"
	grep -E 'OPEN OK|OPEN FAILED|no X server|Xorg|Xvesa' $OUT | head -n 6
	echo "connectors: $(ls /sys/class/drm/ 2>/dev/null | tr '\n' ' ')"
	grep -iE 'no screens found|Failed to load module|open /dev' $OUT | tail -n 6
	echo "========================================================"
} > /dev/tty1 2>&1
