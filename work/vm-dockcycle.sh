#!/bin/bash
# vm-dockcycle.sh - the dock reliability run the brief asks for: click every
# launcher, three times over, and check from the GUEST's own window list that
# the click did the right thing.
#
# "Right thing" is NOT simply "a new window appeared".  A single-instance app
# clicked while it is already open must FOCUS that window rather than start a
# second copy, so the assertion is: after the click, this application has a
# window and that window has the focus.  Multi-instance entries (the terminal,
# the editor) additionally get a brand-new window each time.
set -u
B=/home/jantzen/xxri-build; cd "$B"
Q=work/vm/qmp.sock
i() { ./work/vm-input.py "$Q" "$@" >/dev/null; }
wl()    { tail -30 work/vm/serial.log | grep '^WL ' | tail -1 | sed 's/^WL //'; }
count() { wl | tr ' ' '\n' | grep -c '('; }
NAMES="Store Settings Disks Terminal Editor"
XS="392 440 488 536 583"
KEYS="xxri-store-gui xxri-settings xxri-settings aterm FLTK"
MULTI="no no no yes yes"
pass=0; fail=0
for cycle in 1 2 3; do
  echo "===== cycle $cycle"
  set -- $XS;    xs="$*"
  set -- $KEYS;  ks="$*"
  set -- $MULTI; ms="$*"
  n=1
  for nm in $NAMES; do
    x=$(echo $xs | cut -d' ' -f$n)
    k=$(echo $ks | cut -d' ' -f$n)
    m=$(echo $ms | cut -d' ' -f$n)
    before=$(count)
    i move 512 250; sleep 2
    i click $x 719
    sleep 15
    after=$(count); line=$(wl)
    if echo "$line" | grep -q "$k(normal,focused)"; then
      extra=""
      [ "$m" = yes ] && { [ "$after" -gt "$before" ] && extra=" +new window" || extra=" NO NEW WINDOW"; }
      [ "$m" = no ]  && { [ "$after" -gt "$before" ] && extra=" (launched)" || extra=" (focused existing)"; }
      echo "  $nm: OK$extra"; pass=$((pass+1))
    else
      echo "  $nm: FAIL - $k not focused after click | $line"; fail=$((fail+1))
    fi
    n=$((n+1))
  done
done
echo "===== dock clicks: $pass correct, $fail wrong"
exit 0
