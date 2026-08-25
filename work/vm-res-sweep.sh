#!/bin/bash
# vm-res-sweep.sh RES... - boot at each resolution and capture the core scenes,
# reporting the resolution the guest ACTUALLY came up at (which is not always
# the one asked for: QEMU's virtual GPU does not offer every mode).
set -u
B=/home/jantzen/xxri-build; cd "$B"
for RES in "$@"; do
  echo "=================== $RES"
  ./work/vm-kill.sh >/dev/null
  ./work/vm-run.sh "$RES" >/dev/null 2>&1
  ./work/vm-wait.sh 64 >/dev/null 2>&1
  ./work/vm-shot.sh "$RES-desktop" >/dev/null 2>&1
  ACT=$(identify -format '%wx%h' "work/vm/shots/$RES-desktop.png" 2>/dev/null)
  echo "  requested $RES -> actual $ACT"
  W=${ACT%x*}; H=${ACT#*x}
  export XXRI_RES="$ACT"
  DX=$(( (W-315)/2 + 40 )); DY=$((H-49))
  ./work/vm-input.py work/vm/qmp.sock move $((W-9)) $((H-6)) >/dev/null
  sleep 4; ./work/vm-shot.sh "$RES-cc-hover" >/dev/null 2>&1
  ./work/vm-input.py work/vm/qmp.sock move $((W/2)) $((H/3)) >/dev/null; sleep 3
  ./work/vm-input.py work/vm/qmp.sock click $((DX+47)) $DY >/dev/null
  sleep 16
  ./work/vm-input.py work/vm/qmp.sock move $((W/2)) $((H/3)) >/dev/null; sleep 2
  ./work/vm-shot.sh "$RES-settings" >/dev/null 2>&1
  echo "  scenes: $RES-desktop $RES-cc-hover $RES-settings"
done
exit 0
