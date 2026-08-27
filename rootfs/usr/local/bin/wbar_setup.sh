#!/bin/sh
# wbar_setup.sh - xxri OS Lite dock setup.
#
# The dock is defined ENTIRELY by /usr/local/share/wbar/dot.wbar (the xxri
# canonical dock).  Unlike stock Tiny Core, this script does NOT append the
# per-extension `wbar_update.sh` icons or the on-demand launchers, because
# that pipeline produced duplicate Settings icons and pulled in the old
# Tiny Core control-panel launcher.  Copying dot.wbar verbatim guarantees
# every icon appears exactly once.
. /etc/init.d/tc-functions

TCEDIR=/etc/sysconfig/tcedir
[ -L "$TCEDIR" ] || exit 1

TCEWBAR="/usr/local/tce.icons"

read USER < /etc/sysconfig/tcuser
WBARICONS=/home/"$USER"/.wbar
[ -L "$WBARICONS" ] || ln -s "$TCEWBAR" "$WBARICONS"

# Authoritative base dock: the six xxri launchers from dot.wbar, nothing from
# the fragile per-extension pipeline.
[ -e "$TCEWBAR" ] && sudo rm -rf "$TCEWBAR"
sudo cp /usr/local/share/wbar/dot.wbar "$TCEWBAR"

# re-append every Store-installed app so the dock persists across
# reboots.  Each integrated app has a registry file with its Name and icon;
# xxri-app added the same triplet live at install time, and this rebuilds them
# on boot.  The base dock stays first, apps follow - each appears exactly once.
REG_DIR=/usr/local/share/xxri-apps
if [ -d "$REG_DIR" ]; then
	for r in "$REG_DIR"/*.reg; do
		[ -f "$r" ] || continue
		id=$(basename "$r" .reg)
		grep -q '^INTEGRATED=1' "$r" || continue
		nm=$(sed -n 's/^NAME=//p' "$r" | head -1)
		ic=$(sed -n 's/^ICON=//p' "$r" | head -1)
		[ -n "$nm" ] || nm="$id"
		[ -s "$ic" ] || ic=/usr/local/share/pixmaps/xxri-app-generic.png
		printf 'i: %s\nt: %s\nc: exec xxri-app launch %s\n' "$ic" "$nm" "$id" | sudo tee -a "$TCEWBAR" >/dev/null
	done
fi

sudo chown root:staff "$TCEWBAR"
sudo chmod g+w "$TCEWBAR"
