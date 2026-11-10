#!/bin/sh
# QA ONLY - never shipped. Store install of one app, as the desktop user,
# on the real ISO: progress samples, install trace, xxri-app log, mounts.
# Report on the 2 MiB raw log disk.
L=""
for b in /sys/block/sd? /sys/block/hd?; do
	[ -e "$b/size" ] && [ "$(cat "$b/size")" = 4096 ] && L=${b##*/}
done
U=$(cat /etc/sysconfig/tcuser 2>/dev/null); [ -n "$U" ] || U=tc
say() { echo "$*" > /dev/ttyS0; }
OUT=/tmp/aqd.log
: > $OUT
APP=${AQD_APP:-helium}
session_up() {
	[ -e /tmp/.X11-unix/X0 ] || return 1
	for e in /proc/[0-9]*/environ; do
		grep -qa 'XXRI_DOCK_RESERVE=' "$e" 2>/dev/null && return 0
	done
	return 1
}
i=0
while [ $i -lt 600 ] && ! session_up; do sleep 1; i=$((i + 1)); done
session_up || echo "!!! no session after ${i}s" >> $OUT
sleep 15
sess=""; best=0
for e in /proc/[0-9]*/environ; do
	grep -qa 'XXRI_DOCK_RESERVE=' "$e" 2>/dev/null || continue
	n=$(tr '\0' '\n' < "$e" 2>/dev/null | grep -c '=')
	[ "$n" -gt "$best" ] && { best=$n; sess=${e#/proc/}; sess=${sess%/environ}; }
done
tr '\0' '\n' < /proc/$sess/environ | grep -E '^[A-Za-z_][A-Za-z0-9_]*=' |
	sed "s/'/'\\\\''/g; s/=/='/; s/\$/'/; s/^/export /" > /tmp/session.env
chmod 644 /tmp/session.env
asu() { su "$U" -c ". /tmp/session.env; $*" 2>&1; }
log() { echo "$*" >> $OUT; }
XS=""
for b in /sys/block/sd? /sys/block/hd?; do
	[ -e "$b/size" ] && [ "$(cat "$b/size")" = 131072 ] && XS=${b##*/}
done
XN=0
xshot() { # name: the root window as the X server has it, to slot N of the shots disk
	asu "/opt/aqa/xdump /tmp/xs.ppm" >> $OUT 2>&1
	[ -n "$XS" ] && dd if=/tmp/xs.ppm of=/dev/$XS bs=1M seek=$((XN * 12)) conv=notrunc 2>/dev/null
	sync; say "AQD_XSHOT $XN $1"; XN=$((XN + 1))
}

# the medium boots into the installer's Welcome screen; "Try live ISO first" hands over to the
# real desktop by exiting it and starting the dock and Control Center (session_restore)
pkill xxri-installer; sleep 2
asu "nohup wbar.sh >/dev/null 2>&1 & nohup xxri-control-center >/dev/null 2>&1 &"
sleep 10
log "kernel: $(uname -r)"
log "--- modules"; lsmod | grep -E 'virtio|drm' >> $OUT
log "--- /sys/class/drm"; for c in /sys/class/drm/card*-*; do echo "$c $(cat $c/status 2>/dev/null) modes=$(wc -l < $c/modes 2>/dev/null)"; done >> $OUT 2>&1
log "--- xrandr"; asu "xrandr --query" >> $OUT
log "--- layout"; asu "xxri-display layout; xxri-display layout --json" >> $OUT
say "AQD_SHOT before"; xshot before; sleep 1
say "AQD_UI 1 ${AQD_W2:-1920} ${AQD_H2:-1080}"; sleep 8
log "--- xrandr after the second head was enabled"; asu "xrandr --query | grep -E '^[A-Za-z]|\\*'" >> $OUT
log "--- layout (hotplug applied the saved/default layout)"; asu "xxri-display layout" >> $OUT
say "AQD_SHOT mirror"; xshot mirror; sleep 1
log "--- set-layout extend"; asu "xxri-display set-layout extend --keep" >> $OUT
sleep 15
log "--- xrandr after"; asu "xrandr --query; xrandr --listmonitors" >> $OUT
log "--- layout after"; asu "xxri-display layout" >> $OUT
log "--- root tree (windows >= 100px)"; asu "xwininfo -root -tree" 2>&1 | grep -E 'wbar|dock|xxri|flwm|[0-9]{3,}x[0-9]+\+' | head -40 >> $OUT
log "--- processes"; ps | grep -E 'wbar|flwm|compositor|Xorg|xxri' | grep -v grep >> $OUT
say "AQD_SHOT extend"; xshot extend; sleep 1
log "--- diag"; asu "xxri-display diag /tmp/diag.txt" >> $OUT; cat /tmp/diag.txt >> $OUT 2>&1
log "--- xorg.log"; grep -E 'EE|WW|Output|modeset|Screen' /tmp/xorg.log | tail -40 >> $OUT 2>&1
say "AQD_DONE"
[ -n "$L" ] && { cat $OUT; printf '\0'; } | dd of=/dev/$L bs=512 conv=sync 2>/dev/null
sync; sleep 2; poweroff
