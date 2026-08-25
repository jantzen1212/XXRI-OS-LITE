#!/bin/bash
# vm-csd2.sh - exercise the application-drawn window controls, re-locating them
# before every click.  They move with the window, and clicking where they used
# to be proves nothing (which is how the first version of this test fooled me).
set -u
B=/home/jantzen/xxri-build; cd "$B"; Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
wl() { tail -30 work/vm/serial.log | grep '^WL ' | tail -1 | sed 's/^WL //'; }
# find the three control centres by their XXRI colours, anywhere on screen
locate() {
  ./work/vm-shot.sh csd-loc >/dev/null 2>&1
  magick work/vm/shots/csd-loc.png txt: 2>/dev/null | awk -F'[,:]' '
    {split($0,a,"#"); h=substr(a[2],1,6);
     r=strtonum("0x" substr(h,1,2)); g=strtonum("0x" substr(h,3,2)); b=strtonum("0x" substr(h,5,2));
     if(r>200&&r<245&&g<60&&b>180&&b<225){ if(nx==0){nx=$1; ny=$2} }}
    END{ if(nx) print nx, ny }'
}
act() {   # act LABEL dx  -- click the control dx pixels right of the minimise one
  local lab="$1" dx="$2"
  set -- $(locate)
  if [ -z "${1:-}" ]; then echo "  $lab: controls not found"; return; fi
  local x=$(( $1 + 4 + dx )) y=$(( $2 + 4 ))
  i click $x $y
  sleep 3
  echo "  $lab (clicked $x,$y): $(wl)"
}
echo "== launch"; i move 512 250; sleep 1
echo "   $(wl)"
act "minimise" 0
echo "== restore from dock"; i move 512 250; sleep 1; i click 441 719; sleep 7; echo "  $(wl)"
act "maximise" 15
act "restore " 15
echo "== drag the header"; i drag 150 20 430 250; sleep 2; echo "  moved"
act "close   " 30
exit 0
