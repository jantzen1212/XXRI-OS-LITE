#!/bin/busybox ash
. /etc/init.d/xxri-functions
useBusybox
TMP=/tmp/select.$$
LOCAL=/opt/localmirrors
MIRRORS="/usr/local/share/mirrors"

[ -f "$LOCAL" ] && cat "$LOCAL" > "$TMP"
[ -f "$MIRRORS" ] && cat "$MIRRORS" >> "$TMP"
if [ ! -s "$TMP" ]; then
   echo "Requires mirrors.tcz extension or /opt/localmirrors"
   exit 1
fi
select "xxri OS Lite - Mirror Selection" "$TMP"
ANS="$(cat  /tmp/select.ans)"
rm "$TMP"
[ "$ANS" != "q" ] && echo "$ANS" > /opt/xxri-mirror
