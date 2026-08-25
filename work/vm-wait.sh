#!/bin/bash
# vm-wait.sh SECONDS [NAME] - wait for the guest to settle, then screendump.
# Foreground sleeps are blocked by the agent harness, so every wait in this
# project has to live inside a script file like this one.
set -u
D=/home/jantzen/xxri-build/work/vm
sleep "${1:-55}"
[ -n "${2:-}" ] && /home/jantzen/xxri-build/work/vm-shot.sh "$2"
exit 0
