#!/bin/sh
#(c) Robert Shingledecker 2004-2010
# xxri OS Lite: menu entry only. Stock Tiny Core also added a dock icon here,
# duplicating the pinned ones; the dock is pinned.list's (xxri-dock-pin).
. /etc/init.d/tc-functions
#
[ "$USER" ] || USER="$(cat /etc/sysconfig/tcuser)" || USER="xxri"
if [ "$HOME" == "/root" ]; then HOME=/home/"$USER"; fi
# The following cannot use EXPORTS as also called during boot via tc-setup
DESKTOP=`cat /etc/sysconfig/desktop`
APPNAME="$1"
#
[ $(which "$DESKTOP"_makemenu) ] && "$DESKTOP"_makemenu "$APPNAME" 2>/dev/null
