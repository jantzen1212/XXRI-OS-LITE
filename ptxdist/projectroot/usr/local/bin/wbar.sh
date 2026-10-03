#!/bin/sh
# wbar.sh - xxri OS Lite dock launcher: (re)start wbar on tce.icons.
#
# No `replace .wbarconf ... 'c: wbar'` like stock Tiny Core: it matched any
# line containing "c: wbar" and the extra c: line shifted every dock icon onto
# the previous icon's command. xxri-dock-pin writes the options line already.
. /etc/init.d/tc-functions
[ -n "$ICONS" ] || ICONS=$(cat /etc/sysconfig/icons 2>/dev/null)
# Only with X up: setupdesktop calls this before X exists, and the wait
# below then burned ~2.4s on a wbar that could never start.
if [ "$ICONS" = "wbar" ] && [ -e /tmp/.X11-unix/X0 ]; then
	pidof wbar >/dev/null && killall wbar 2>/dev/null
	nohup wbar >/dev/null 2>&1 &
	# wbar must be fully started or the parent dying would kill it.
	for i in $(seq 100); do
		pidof wbar >/dev/null && break
		sleep 0.02
	done
fi
