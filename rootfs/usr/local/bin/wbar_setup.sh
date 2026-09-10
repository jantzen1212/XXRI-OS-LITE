#!/bin/sh
# wbar_setup.sh - xxri OS Lite dock setup, run at boot before wbar itself
# starts (see desktop.sh / setupdesktop / .xsession).
#
# The dock's PINNED area is defined by /usr/local/share/xxri-launcher/
# pinned.list (one .desktop id per line, max 8), turned into the live
# /usr/local/tce.icons file by xxri-dock-pin - the same tool the All Apps
# drawer calls live when the user pins or unpins an app.  This script only
# has to set up the symlink dance wbar itself expects and hand off to that
# tool, so cold boot and a live pin/unpin build the dock exactly the same way.
#
# Newly installed/downloaded applications are NOT added here or anywhere
# else automatically - they show up in the All Apps drawer (which reads
# .desktop files directly) and reach the dock only if the user pins them.
. /etc/init.d/tc-functions

TCEDIR=/etc/sysconfig/tcedir
[ -L "$TCEDIR" ] || exit 1

TCEWBAR="/usr/local/tce.icons"

# startx calls this script and then calls setupdesktop, which calls it a second
# time with nothing changed in between, rebuilding an identical dock.  Both of
# those callers are shipped inside desktop .tcz files, so the redundant run is
# short-circuited here.  /tmp is tmpfs, so the stamp lasts exactly one boot and
# a later pin/unpin (xxri-dock-pin) is unaffected -- it does not come through
# this script.
XXRI_WBAR_STAMP=/tmp/.xxri-wbar-setup-done
[ -f "$XXRI_WBAR_STAMP" ] && [ -s "$TCEWBAR" ] && exit 0

read USER < /etc/sysconfig/tcuser
WBARICONS=/home/"$USER"/.wbar
[ -L "$WBARICONS" ] || ln -s "$TCEWBAR" "$WBARICONS"

if [ -x /usr/local/bin/xxri-dock-pin ]; then
	/usr/local/bin/xxri-dock-pin boot
else
	# Fallback so a broken/missing xxri-dock-pin still boots to a dock
	# instead of none at all: wbar's own header triple, no pinned apps.
	sed -n '1,/^c: wbar /p' /usr/local/share/wbar/dot.wbar | grep -v '^#' | sudo tee "$TCEWBAR" >/dev/null
fi

sudo chown root:staff "$TCEWBAR"
sudo chmod g+w "$TCEWBAR"

# Mark this boot's dock as built (see the stamp check above).
touch "$XXRI_WBAR_STAMP" 2>/dev/null || true
