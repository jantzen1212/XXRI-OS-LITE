#!/bin/bash
# vm-cc-stress.sh - open and close the Control Center repeatedly with real
# pointer motion, to prove the reveal animation cannot leave it stuck, doubled
# or collapsed-on-open.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
W=1024; H=768
for n in $(seq 1 8); do
  i move $((W-9)) $((H-6)); sleep 1
  i move $((W/2)) $((H/3)); sleep 1
done
i move $((W-9)) $((H-6)); sleep 3
./work/vm-shot.sh cc-stress-final >/dev/null 2>&1
echo "final shot taken"
exit 0
