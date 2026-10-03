#!/bin/sh
# wbar_setup.sh - xxri OS Lite dock setup, run at boot before wbar starts.
#
# The dock comes from pinned.list via xxri-dock-pin, the same path a live
# pin/unpin takes; this only adds the ~/.wbar symlink wbar expects. New apps
# never land on the dock by themselves, only in All Apps.
. /etc/init.d/tc-functions

TCEDIR=/etc/sysconfig/tcedir
[ -L "$TCEDIR" ] || exit 1

TCEWBAR="/usr/local/tce.icons"

# startx runs this, then setupdesktop runs it again for nothing. Both are
# upstream, so skip the second run here (the stamp lives in tmpfs: one boot).
XXRI_WBAR_STAMP=/tmp/.xxri-wbar-setup-done
[ -f "$XXRI_WBAR_STAMP" ] && [ -s "$TCEWBAR" ] && exit 0

read USER < /etc/sysconfig/tcuser
WBARICONS=/home/"$USER"/.wbar
[ -L "$WBARICONS" ] || ln -s "$TCEWBAR" "$WBARICONS"

if [ -x /usr/local/bin/xxri-dock-pin ]; then
	/usr/local/bin/xxri-dock-pin boot
else
	# no xxri-dock-pin: still a dock, just wbar's header and no apps
	sed -n '1,/^c: wbar /p' /usr/local/share/wbar/dot.wbar | grep -v '^#' | sudo tee "$TCEWBAR" >/dev/null
fi

sudo chown root:staff "$TCEWBAR"
sudo chmod g+w "$TCEWBAR"

touch "$XXRI_WBAR_STAMP" 2>/dev/null || true
