#!/bin/sh
# wbar.sh - xxri OS Lite dock launcher.
#
# Stock Tiny Core patched the dock options line by running
#   replace .wbarconf /usr/local/tce.icons 'c: wbar'
# which does a *substring* replacement: any line CONTAINING "c: wbar"
# (including a comment that merely mentions it) is turned into a real
# `c: wbar ...` entry.  That injected an extra `c:` line into tce.icons, so
# wbar - which pairs its icon and command arrays by index - shifted every
# launcher by one slot (each icon ran the previous icon's command, and the
# last icon was dropped).  The authoritative dock (/usr/local/share/wbar/
# dot.wbar, copied verbatim by wbar_setup.sh) already contains the correct
# options line, so we simply (re)launch wbar against tce.icons with no
# fragile string surgery.
. /etc/init.d/tc-functions
[ -n "$ICONS" ] || ICONS=$(cat /etc/sysconfig/icons 2>/dev/null)
if [ "$ICONS" = "wbar" ]; then
	pidof wbar >/dev/null && killall wbar 2>/dev/null
	nohup wbar >/dev/null 2>&1 &
	# wbar must be fully started or the parent dying would kill it.
	for i in $(seq 100); do
		pidof wbar >/dev/null && break
		sleep 0.02
	done
fi
