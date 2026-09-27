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
i=0
while [ $i -lt 1200 ] && ! { [ -e /tmp/.X11-unix/X0 ] && pidof wbar >/dev/null 2>&1; }; do
	sleep 1; i=$((i + 1))
done
sleep 15
{
	echo "XQA desktop_wait=${i}s"
	echo "--- cpu: $(grep -m1 'model name' /proc/cpuinfo) sse2=$(grep -qw sse2 /proc/cpuinfo && echo yes || echo no)"
	echo "--- cmdline: $(cat /proc/cmdline)"
	echo "--- server"
	for s in Xorg Xfbdev Xvesa; do
		p=$(pidof $s) && echo "SERVER=$s pid=$p args=$(tr '\0' ' ' < /proc/${p%% *}/cmdline) rss_kb=$(sed -n 's/^VmRSS:[^0-9]*\([0-9]*\).*/\1/p' /proc/${p%% *}/status)"
	done
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
		grep -E '\(EE\)|\(WW\)|XLibre|X.Org X Server|modeset\(0\)|glamor|GLX|AIGLX|libinput|AccelMethod|Using config|Loading|UnloadModule|Disabling|Output .* connected' /var/log/Xorg.0.log | head -80
	fi
	[ -f /tmp/xfbdev.log ] && head -40 /tmp/xfbdev.log
	[ -f /tmp/xorg.log ] && grep -E 'EE|Fatal|error' /tmp/xorg.log | head -10
	echo "--- memory"; free
} > $OUT 2>&1
# input: XInput2 events while the host injects
su "$U" -c "DISPLAY=:0 XAUTHORITY=$XAUTHORITY xinput test-xi2 --root" > /tmp/xi2.log 2>&1 &
XI=$!
sleep 3
say "XQA_READY"
sleep 30
kill $XI 2>/dev/null
{
	echo "--- input"
	echo "XI_MOTION=$(grep -c 'EVENT type 6 (Motion)' /tmp/xi2.log)"
	echo "XI_BUTTONPRESS=$(grep -c 'EVENT type 4 (ButtonPress)' /tmp/xi2.log)"
	echo "XI_KEYPRESS=$(grep -c 'EVENT type 2 (KeyPress)' /tmp/xi2.log)"
	echo "XI_KEY_A=$(grep -A3 'EVENT type 2 (KeyPress)' /tmp/xi2.log | grep -c 'detail: 38$')"
	echo "XI_RAW=$(grep -c 'EVENT type 1[3-9] (Raw' /tmp/xi2.log)"
	head -c 1500 /tmp/xi2.log
	echo "XQA_END"
} >> $OUT 2>&1
# apps (xxriqaapps): every XXRI program in turn; alive after its settle time
# = started without faulting (an instruction the CPU lacks kills it: SIGILL)
if grep -qw xxriqaapps /proc/cmdline; then
	T=$(sed -n 's/.*xxriqaapps=\([0-9]*\).*/\1/p' /proc/cmdline); [ -n "$T" ] || T=25
	echo "--- apps (settle ${T}s)" >> $OUT
	for app in xxri-settings xxri-store-gui xxri-control-center xxri-launcher xxri-media \
		   xxri-file xxri-power-menu "xxri-dialog info QA check" xxri-installer xxri-browser; do
		name=${app%% *}
		su "$U" -c "DISPLAY=:0 XAUTHORITY=$XAUTHORITY $app" > /tmp/app-$name.log 2>&1 &
		sleep "$T"
		if pgrep -f "$name" >/dev/null 2>&1; then st=alive; else st=exited; fi
		say "XQA_APP $name"
		sleep 4
		echo "APP $name $st $(tail -c 300 /tmp/app-$name.log | tr '\n' ' ')" >> $OUT
		pkill -f "$name" 2>/dev/null
		sleep 2
	done
fi
[ -n "$L" ] && dd if=$OUT of=/dev/$L bs=4k conv=sync 2>/dev/null
sync
say "XQA_DONE"
sleep 8
poweroff
