#!/bin/bash
# vm-dock34.sh - acceptance for subsystems 3 and 4, with real pointer input.
#   * an indicator appears when an application starts and goes when it exits
#   * a minimised application keeps its indicator and stays discoverable
#   * clicking its dock icon RESTORES it instead of starting a second copy
set -u
B=/home/jantzen/xxri-build; cd "$B"
Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
park() { i move 512 250; sleep 2; }
shot() { ./work/vm-shot.sh "$1" >/dev/null 2>&1; echo "   shot $1"; }
wl() { tail -40 work/vm/serial.log | grep '^WL ' | tail -1 | sed 's/^WL //'; }

echo "== 1. clean desktop, nothing running"
park; shot d34-01-idle;              echo "   windows: $(wl)"
echo "== 2. launch Settings from the dock"
i click 441 719; sleep 14; park;     shot d34-02-settings-running
echo "   windows: $(wl)"
echo "== 3. launch the Terminal too"
i click 535 719; sleep 8; park;      shot d34-03-two-running
echo "   windows: $(wl)"
echo "== 4. minimise Settings with its own window control"
# Settings is undecorated now and draws its controls itself, at the top-left of
# its sidebar - flwm's old titlebar coordinates no longer mean anything here.
i click 7 11; sleep 2; park;         shot d34-04-settings-minimised
echo "   windows: $(wl)"
echo "== 5. click the Settings dock icon: must RESTORE, not open a second copy"
i click 441 719; sleep 8; park;      shot d34-05-restored
echo "   windows: $(wl)"
echo "== 6. click it again while it is already up: must just focus it"
i click 441 719; sleep 8; park;      shot d34-06-focused-again
echo "   windows: $(wl)"
echo "== done"
exit 0
