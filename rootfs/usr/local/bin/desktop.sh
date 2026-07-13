#!/bin/sh
#(c) Robert Shingledecker 2004-2010
# xxri OS Lite (Phase 8.1): register an app in the Applications MENU only.
#
# Stock Tiny Core also appended a wbar dock icon here (via wbar_update.sh).
# setupdesktop calls this for every non-tinycore .desktop AFTER wbar_setup.sh
# has already laid down the authoritative dock (dot.wbar), which re-added
# duplicate Settings / Terminal / Wifi icons to the dock.  The xxri dock is
# defined SOLELY by /usr/local/share/wbar/dot.wbar, so we build the menu entry
# and deliberately do NOT touch the dock.
#
. /etc/init.d/tc-functions
#
[ "$USER" ] || USER="$(cat /etc/sysconfig/tcuser)" || USER="xxri"
if [ "$HOME" == "/root" ]; then HOME=/home/"$USER"; fi
# The following cannot use EXPORTS as also called during boot via tc-setup
DESKTOP=`cat /etc/sysconfig/desktop`
APPNAME="$1"
#
[ $(which "$DESKTOP"_makemenu) ] && "$DESKTOP"_makemenu "$APPNAME" 2>/dev/null
