#!/bin/sh
# QA ONLY - never shipped.  Appended as a second initrd from GRUB's editor by
# tools/qa/x-test.py.  Waits for the desktop, reports which X server runs and
# what it offers, records XInput2 events while the host injects keys and mouse
# movement through QEMU, writes everything to the 2 MiB raw "log disk", powers
# off.
L=""
for b in /sys/block/sd? /sys/block/hd?; do
	[ -e "$b/size" ] && [ "$(cat "$b/size")" = 4096 ] && L=${b##*/}
done
U=$(cat /etc/sysconfig/tcuser 2>/dev/null); [ -n "$U" ] || U=tc
export DISPLAY=:0 XAUTHORITY=/home/$U/.Xauthority
say() { echo "$*" > /dev/ttyS0; }
OUT=/tmp/xqa.log
say "XQA_HOOK"
# wait for the desktop; `xxriqawait=N` (default 1200s, TCG runs are slow),
# and report either way - a machine without a desktop needs the logs most
W=$(sed -n 's/.*xxriqawait=\([0-9]*\).*/\1/p' /proc/cmdline); [ -n "$W" ] || W=1200
# the desktop is up when the X socket exists and ~/.xsession has started its
# programs (they carry its XXRI_DOCK_RESERVE export).  Not by the dock's
# name: the live first-run screen replaces wbar moments after it starts.
session_up() {
	[ -e /tmp/.X11-unix/X0 ] || return 1
	for e in /proc/[0-9]*/environ; do
		grep -qa 'XXRI_DOCK_RESERVE=' "$e" 2>/dev/null && return 0
	done
	return 1
}
i=0
while [ $i -lt $W ] && ! session_up; do
	sleep 1; i=$((i + 1))
done
if session_up; then DESKTOP=yes; sleep 15; else DESKTOP=no; fi
{
	echo "XQA desktop=$DESKTOP desktop_wait=${i}s"
	echo "--- cpu: $(grep -m1 'model name' /proc/cpuinfo) sse2=$(grep -qw sse2 /proc/cpuinfo && echo yes || echo no)"
	echo "--- cmdline: $(cat /proc/cmdline)"
	echo "--- server"
	for s in Xorg Xfbdev Xvesa; do
		p=$(pidof $s) && echo "SERVER=$s pid=$p args=$(tr '\0' ' ' < /proc/${p%% *}/cmdline) rss_kb=$(sed -n 's/^VmRSS:[^0-9]*\([0-9]*\).*/\1/p' /proc/${p%% *}/status)" &&
			echo "  $(grep -E '^(Uid|Gid):' /proc/${p%% *}/status | tr '\t\n' '  ')" &&
			echo "  open: $(ls -l /proc/${p%% *}/fd 2>/dev/null | sed -n 's/.*-> //p' | grep -E 'input|dri|fb|tty|mice|psaux' | tr '\n' ' ')"
	done
	echo "--- processes"; ps -o pid,user,comm,args 2>/dev/null | grep -vE ' \[|ps -o' | cut -c1-150
	echo "--- nodes"; ls -l /dev/input/ /dev/dri/ /dev/fb0 /dev/tty7 2>&1 | grep -v '^total'
	echo "--- /dev/dri: $(ls /dev/dri 2>/dev/null | tr '\n' ' ') /proc/fb: $(cat /proc/fb 2>/dev/null | tr '\n' ' ')"
	echo "--- drm modules: $(lsmod 2>/dev/null | awk '/drm|bochs|cirrus|simpledrm|radeon|vmwgfx|i915|virtio_gpu/{print $1}' | tr '\n' ' ')"
	echo "--- kernel display"; dmesg 2>/dev/null | grep -iE 'drm|radeon|vmwgfx|cirrus|bochs|fb0|vesafb|efifb|simplefb' | tail -25
	echo "--- xorg.conf.d"; ls /etc/X11/xorg.conf.d /usr/local/share/X11/xorg.conf.d 2>/dev/null
	cat /etc/X11/xorg.conf.d/*.conf 2>/dev/null
	echo "--- xdpyinfo"
	xdpyinfo 2>&1 | sed -n '/^vendor string/p;/^version number/p;/^X.Org version/p;/^XLibre/p;/^  dimensions/p;/^  depth of root/p;/number of extensions/,/^default screen/p'
	echo "--- xrandr"; xrandr 2>&1 | head -12
	echo "--- setxkbmap"; setxkbmap -query 2>&1
	echo "--- xinput"; xinput list 2>&1
	echo "--- server log"
	if [ -f /var/log/Xorg.0.log ]; then
		grep -E '\(EE\)|\(WW\)|XLibre|X.Org X Server|modeset\(0\)|glamor|GLX|AIGLX|libinput|AccelMethod|Using config|Loading|UnloadModule|Disabling|Output .* connected|VESA|VBE|int10|Matched .* as autoconfigured' /var/log/Xorg.0.log | head -100
	fi
	[ -f /tmp/xorg.log ] && { echo "--- /tmp/xorg.log (tail)"; tail -40 /tmp/xorg.log; }
	[ -f /tmp/xfbdev.log ] && { echo "--- /tmp/xfbdev.log"; head -60 /tmp/xfbdev.log; }
	echo "--- memory"; free
} > $OUT 2>&1
if [ $DESKTOP = no ]; then
	# one more server start, verbose, for what the default log level hides
	# (int10 / VBE / video BIOS access)
	if [ -x /usr/local/lib/xorg/Xorg ]; then
		timeout 25 /usr/local/lib/xorg/Xorg :5 -logverbose 7 -logfile /tmp/xorg-diag.log -nolisten tcp vt8 >/dev/null 2>&1
		{ echo "--- verbose Xorg (diagnostic run)"
		  grep -iE 'int10|vbe|vesa|bios|rom|/dev/mem|legacy|x86emu|segment|pci' /tmp/xorg-diag.log | head -80
		  echo "--- /dev/mem: $(ls -l /dev/mem 2>&1)  iomem video: $(grep -iE 'video|vga|c0000' /proc/iomem | tr '\n' ';')"
		} >> $OUT 2>&1
	fi
	[ -n "$L" ] && dd if=$OUT of=/dev/$L bs=4k conv=sync 2>/dev/null
	sync; say "XQA_NODESKTOP"; say "XQA_DONE"; sleep 8; poweroff; exit 0
fi
# input: XInput2 events while the host injects
su "$U" -c "DISPLAY=:0 XAUTHORITY=$XAUTHORITY xinput test-xi2 --root" > /tmp/xi2.log 2>&1 &
XI=$!
# what the kernel delivers, independent of the X server: every reader of an
# input node gets its own copy of the stream
RAW=""
for n in /dev/input/mice /dev/input/event*; do
	cat "$n" > "/tmp/raw-${n##*/}" 2>/dev/null & RAW="$RAW $!"
done
sleep 3
say "XQA_READY"
sleep 30
kill $XI $RAW 2>/dev/null
{
	echo "--- input"
	echo "XI_MOTION=$(grep -c 'EVENT type 6 (Motion)' /tmp/xi2.log)"
	echo "XI_BUTTONPRESS=$(grep -A2 'EVENT type 4 (ButtonPress)' /tmp/xi2.log | grep -c 'detail: [123]$')"
	echo "XI_WHEEL=$(grep -A2 'EVENT type 4 (ButtonPress)' /tmp/xi2.log | grep -c 'detail: [45]$')"
	echo "XI_KEYPRESS=$(grep -c 'EVENT type 2 (KeyPress)' /tmp/xi2.log)"
	echo "XI_KEY_A=$(grep -A3 'EVENT type 2 (KeyPress)' /tmp/xi2.log | grep -c 'detail: 38$')"
	echo "XI_RAW=$(grep -c 'EVENT type 1[3-9] (Raw' /tmp/xi2.log)"
	echo "XI_RAWMOTION=$(grep -c 'EVENT type 17 (RawMotion)' /tmp/xi2.log)"
	for r in /tmp/raw-*; do echo "KERNEL ${r#/tmp/raw-} bytes=$(wc -c < "$r") name=$(cat /sys/class/input/${r#/tmp/raw-}/device/name 2>/dev/null)"; done
	head -c 1500 /tmp/xi2.log
	echo "XQA_END"
} >> $OUT 2>&1
# apps (xxriqaapps): every XXRI program in turn; alive after its settle time
# = started without faulting (an instruction the CPU lacks kills it: SIGILL)
if grep -qw xxriqaapps /proc/cmdline; then
	T=$(sed -n 's/.*xxriqaapps=\([0-9]*\).*/\1/p' /proc/cmdline); [ -n "$T" ] || T=25
	echo "--- apps (settle ${T}s)" >> $OUT
	# resident memory of every process of an app (the browser: its engine
	# processes too), in kB
	rss_of() {
		t=0
		for p in $(pgrep -f "$1"); do
			r=$(sed -n 's/^VmRSS:[^0-9]*\([0-9]*\).*/\1/p' /proc/$p/status 2>/dev/null)
			t=$((t + ${r:-0}))
		done
		echo $t
	}
	# the session's own environment (what ~/.xsession exported for everything
	# it starts - the dock launches apps with it), taken from the window
	# manager and quoted for the shell
	# (any process ~/.xsession started: they all carry its exports - found by
	# one of them, XXRI_DOCK_RESERVE, not by a program name that may be
	# restarting at this moment)
	# ... and of those the one with the most variables: ~/.xsession only ever
	# adds exports, so the program it started last has all of them (the first
	# ones predate xxri-session-runtime)
	sess=""; best=0
	for e in /proc/[0-9]*/environ; do
		grep -qa 'XXRI_DOCK_RESERVE=' "$e" 2>/dev/null || continue
		n=$(tr '\0' '\n' < "$e" 2>/dev/null | grep -c '=')
		[ "$n" -gt "$best" ] && { best=$n; sess=${e#/proc/}; sess=${sess%/environ}; }
	done
	tr '\0' '\n' < /proc/$sess/environ | grep -E '^[A-Za-z_][A-Za-z0-9_]*=' |
		sed "s/'/'\\\\''/g; s/=/='/; s/\$/'/; s/^/export /" > /tmp/session.env
	echo "--- session env: $(wc -l < /tmp/session.env) variables from pid $sess ($(cat /proc/$sess/comm 2>/dev/null)); Qt: $(grep -oE '^export QT[A-Z_]*' /tmp/session.env | sed 's/export //' | tr '\n' ' ')" >> $OUT
	for app in xxri-settings xxri-store-gui xxri-control-center xxri-launcher xxri-media \
		   xxri-file xxri-power-menu "xxri-dialog info QA check" xxri-installer xxri-browser \
		   "browser-https=xxri-browser https://example.org/"; do
		case "$app" in
		*=*) name=${app%%=*}; cmd=${app#*=} ;;
		*)   name=${app%% *}; cmd=$app ;;
		esac
		pat=${cmd%% *}; [ "$pat" = xxri-browser ] && pat='xxri-browser|QtWebEngineProcess'
		su "$U" -c ". /tmp/session.env; $cmd" > /tmp/app-$name.log 2>&1 &
		sleep "$T"
		if pgrep -f "$pat" >/dev/null 2>&1; then st=alive; else st=exited; fi
		rss=$(rss_of "$pat")
		say "XQA_APP $name"
		sleep 4
		echo "APP $name $st rss_kb=$rss $(tail -c 300 /tmp/app-$name.log | tr '\n' ' ')" >> $OUT
		pkill -f "$pat" 2>/dev/null
		sleep 2
	done
	{ echo "--- memory after apps"; free; } >> $OUT
fi
[ -n "$L" ] && dd if=$OUT of=/dev/$L bs=4k conv=sync 2>/dev/null
sync
say "XQA_DONE"
sleep 8
poweroff
