#!/bin/bash
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
i move 512 250; sleep 2
( i click 536 719 & )
sleep 3; ./work/vm-shot.sh a2-launch-flight >/dev/null 2>&1
sleep 6; ./work/vm-shot.sh a2-launched >/dev/null 2>&1
( i click 18 13 & ); sleep 1.2; ./work/vm-shot.sh a2-min-flight >/dev/null 2>&1
sleep 5
( i click 536 719 & ); sleep 1.2; ./work/vm-shot.sh a2-restore-flight >/dev/null 2>&1
sleep 5; ./work/vm-shot.sh a2-restored >/dev/null 2>&1
echo done
