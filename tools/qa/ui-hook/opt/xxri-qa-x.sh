#!/bin/sh
# QA ONLY - never shipped.  tools/qa/ui26-test.py appends this as a second
# initrd (the x-test mechanism, same file name and trigger).  In the live
# session it walks the 2.6 UI work - Settings > Wallpaper & Style, Software
# Update, Display, About; wallpapers painted by xxri-wallpaper; the dock
# sizes; dark mode - announcing each scene on the serial line (XQA_APP <name>)
# for the host to screenshot, and writes the backends' answers to the log disk.
L=""
for b in /sys/block/sd? /sys/block/hd?; do
	[ -e "$b/size" ] && [ "$(cat "$b/size")" = 4096 ] && L=${b##*/}
done
U=$(cat /etc/sysconfig/tcuser 2>/dev/null); [ -n "$U" ] || U=tc
export DISPLAY=:0 XAUTHORITY=/home/$U/.Xauthority
say() { echo "$*" > /dev/ttyS0; }
OUT=/tmp/xqa.log
say "XQA_HOOK"
session_up() {
	[ -e /tmp/.X11-unix/X0 ] || return 1
	for e in /proc/[0-9]*/environ; do
		grep -qa 'XXRI_DOCK_RESERVE=' "$e" 2>/dev/null && return 0
	done
	return 1
}
i=0
while [ $i -lt 600 ] && ! session_up; do sleep 1; i=$((i + 1)); done
if ! session_up; then
	echo "XQA desktop=no" > $OUT
	[ -n "$L" ] && dd if=$OUT of=/dev/$L bs=4k conv=sync 2>/dev/null
	sync; say "XQA_DONE"; sleep 5; poweroff; exit 0
fi
sleep 20
# the session's own environment (see x-hook)
sess=""; best=0
for e in /proc/[0-9]*/environ; do
	grep -qa 'XXRI_DOCK_RESERVE=' "$e" 2>/dev/null || continue
	n=$(tr '\0' '\n' < "$e" 2>/dev/null | grep -c '=')
	[ "$n" -gt "$best" ] && { best=$n; sess=${e#/proc/}; sess=${sess%/environ}; }
done
tr '\0' '\n' < /proc/$sess/environ | grep -E '^[A-Za-z_][A-Za-z0-9_]*=' |
	sed "s/'/'\\\\''/g; s/=/='/; s/\$/'/; s/^/export /" > /tmp/session.env
as_user() { su "$U" -c ". /tmp/session.env; $*"; }
# the live first-run installer owns the screen and has stopped the dock and
# the Control Center: do what its "Continue Live Session" does
kill $(pidof xxri-installer) 2>/dev/null
sleep 1
as_user "nohup wbar.sh >/dev/null 2>&1 & nohup xxri-control-center >/dev/null 2>&1 &"
sleep 6
W=/usr/local/share/xxri-theme/wallpapers
scene() { say "XQA_APP $1"; sleep 5; }
app() { # name command
	as_user "$2" > /tmp/app-$1.log 2>&1 &
	sleep 12
	scene "$1"
	echo "APP $1 $(pgrep -f "${2%% *}" >/dev/null && echo alive || echo exited) $(tail -c 200 /tmp/app-$1.log | tr '\n' ' ')" >> $OUT
	pkill -f "${2%% *}" 2>/dev/null; sleep 2
}
{
	echo "XQA desktop=yes desktop_wait=${i}s"
	echo "--- os-release"; grep -E '^(PRETTY_NAME|IMAGE_VERSION|BUILD_ID)=' /etc/os-release
	echo "--- env"; grep -E 'GTK_THEME|XCURSOR|GDK_DPI' /tmp/session.env
	echo "--- xxri-wallpaper"; ls -l /usr/local/bin/xxri-wallpaper; ldd /usr/local/bin/xxri-wallpaper 2>&1 | grep -c 'not found' | sed 's/^/libs not found: /'
	echo "--- monitors"; as_user "xxri-display monitors --json"
	echo "--- wallpaper list"; as_user "xxri-desktop wallpaper list --json"
	echo "--- status"; as_user "xxri-desktop status --json"
	echo "--- themes"; as_user "xxri-desktop themes --json"
	echo "--- setbackground"; cat /home/$U/.setbackground
	echo "--- root pixmap"; xprop -root _XROOTPMAP_ID ESETROOT_PMAP_ID 2>&1
	echo "--- dock header"; sed -n '1,3p' /usr/local/tce.icons
	echo "--- update status"; as_user "xxri-system-update status --json"
	echo "--- update check"; as_user "xxri-system-update check --json"
} > $OUT 2>&1
scene desktop-default
app settings-wallpaper "xxri-settings wallpaper"
{ echo "--- set wallpaper3"; s=$(date +%s); as_user "xxri-desktop wallpaper set $W/wallpaper3.jpg"; echo "rc=$? took=$(( $(date +%s) - s ))s"; } >> $OUT 2>&1
scene wallpaper-crater
{ echo "--- set wallpaper2 + fit"; as_user "xxri-desktop wallpaper set $W/wallpaper2.jpg fit"; echo "rc=$?"; } >> $OUT 2>&1
scene wallpaper-fit
{ echo "--- dock large"; as_user "xxri-desktop dock size large"; sleep 4; sed -n '3p' /usr/local/tce.icons; } >> $OUT 2>&1
scene dock-large
{ echo "--- dock small"; as_user "xxri-desktop dock size small"; sleep 4; sed -n '3p' /usr/local/tce.icons; } >> $OUT 2>&1
scene dock-small
{ as_user "xxri-desktop dock size medium"; as_user "xxri-desktop wallpaper reset"; } >> $OUT 2>&1
sleep 4
# two displays: RandR 1.5 user monitors split the one output in halves (the
# way an ultrawide is split) - a different wallpaper on each half, and the
# Settings preview with two numbered displays
O=$(xrandr 2>/dev/null | awk '/ connected/{print $1; exit}')
if [ -n "$O" ] && xrandr --setmonitor XXRI-L 512/135x768/203+0+0 "$O" 2>>$OUT &&
   xrandr --setmonitor XXRI-R 512/135x768/203+512+0 none 2>>$OUT; then
	{ echo "--- two displays"; xrandr --listactivemonitors; as_user "xxri-display monitors --json"
	  as_user "xxri-desktop wallpaper set $W/wallpaper1.jpg --monitor 1"
	  as_user "xxri-desktop wallpaper set $W/wallpaper3.jpg --monitor 2"
	  grep WALLPAPER /usr/local/etc/xxri/desktop.conf; } >> $OUT 2>&1
	scene two-displays-desktop
	app two-displays-settings "xxri-settings wallpaper"
	xrandr --delmonitor XXRI-L; xrandr --delmonitor XXRI-R
	as_user "xxri-desktop wallpaper reset" >> $OUT 2>&1
	sleep 3
fi
app settings-update "xxri-settings update"
app settings-display "xxri-settings display"
app settings-about "xxri-settings about"
{ echo "--- dark"; as_user "xxri-desktop appearance set dark"; grep dark /home/$U/.config/gtk-3.0/settings.ini; } >> $OUT 2>&1
app settings-dark "xxri-settings wallpaper"
app files-while-dark "xxri-file"
{ echo "--- cursor + theme"; as_user "xxri-desktop theme set Adwaita"; as_user "xxri-desktop cursor set xxri-cursors"; as_user "xxri-desktop scale set 125"
  grep -E 'theme-name|cursor' /home/$U/.config/gtk-3.0/settings.ini; grep Xcursor /home/$U/.Xdefaults; cat /home/$U/.icons/default/index.theme
  echo "--- session scale (next login)"; su "$U" -c 'sh -lc "echo GDK_DPI_SCALE=\$GDK_DPI_SCALE"'
  echo "--- memory"; free; } >> $OUT 2>&1
[ -n "$L" ] && dd if=$OUT of=/dev/$L bs=4k conv=sync 2>/dev/null
sync
say "XQA_DONE"
sleep 8
poweroff
