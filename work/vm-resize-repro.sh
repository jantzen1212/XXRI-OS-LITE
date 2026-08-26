#!/bin/bash
# vm-resize-repro.sh APP - hold the bottom-right resize corner and drag through
# several positions WITHOUT releasing, capturing each intermediate frame.  This
# is the exact gesture that produced stale white control surfaces.
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
TAG="${1:-settings}"
# window is 980x660 at 0,0; the corner handle is the last 14px
i press 974 654; sleep 1
n=1
for p in "900 600" "820 540" "740 470" "660 400"; do
  set -- $p
  i move $1 $2; sleep 1
  ./work/vm-shot.sh "RZ-$TAG-$n" >/dev/null 2>&1; echo "  RZ-$TAG-$n  (pointer $1,$2)"
  n=$((n+1))
done
i release; sleep 2
./work/vm-shot.sh "RZ-$TAG-done" >/dev/null 2>&1; echo "  RZ-$TAG-done"
