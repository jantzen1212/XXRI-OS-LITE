#!/bin/sh
# wbar_setup.sh - xxri OS Lite (Phase 8.1).
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

# Authoritative dock: replace tce.icons with dot.wbar, nothing appended.
[ -e "$TCEWBAR" ] && sudo rm -rf "$TCEWBAR"
sudo cp /usr/local/share/wbar/dot.wbar "$TCEWBAR"
sudo chown root:staff "$TCEWBAR"
sudo chmod g+w "$TCEWBAR"
