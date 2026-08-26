#!/bin/bash
set -e
B=/home/jantzen/xxri-build
SHOTS=$B/work/vm/shots
VMINPUT=$B/work/vm-input.py
VMSHOT=$B/work/vm-shot.sh
Q=$B/work/vm/qmp.sock

before=$SHOTS/settings-before.png
after=$SHOTS/settings-after.png

echo 'computing diff'
magick "$before" "$after" -compose difference -composite "$SHOTS/diff.png"
# create BW masks of before and after to intersect with diff
magick "$before" -colorspace gray -threshold 5% "$SHOTS/bw_before.png"
magick "$after" -colorspace gray -threshold 5% "$SHOTS/bw_after.png"
# mask old and new
magick "$SHOTS/diff.png" "$SHOTS/bw_before.png" -compose multiply -composite "$SHOTS/mask_old.png"
magick "$SHOTS/diff.png" "$SHOTS/bw_after.png" -compose multiply -composite "$SHOTS/mask_new.png"

# get trimmed geometry
geom_old=$(magick "$SHOTS/mask_old.png" -threshold 10% -trim -format "%@" info: || true)
geom_new=$(magick "$SHOTS/mask_new.png" -threshold 10% -trim -format "%@" info: || true)

echo geom_old=$geom_old
echo geom_new=$geom_new

parse(){ IFS='x+' read W H L T <<< "$1"; echo "$L $T $W $H"; }
if [ -n "$geom_old" ]; then
  read L1 T1 W1 H1 <<< $(parse "$geom_old")
else
  L1=0; T1=0
fi
if [ -n "$geom_new" ]; then
  read L2 T2 W2 H2 <<< $(parse "$geom_new")
else
  L2=0; T2=0
fi

echo tl before: $L1,$T1  after: $L2,$T2

# control offsets
declare -A offs_x offs_y
offs_x[close]=30; offs_y[close]=18
offs_x[max]=60; offs_y[max]=18
offs_x[min]=90; offs_y[min]=18

out=$B/work/vm/control-click-results-2.txt
: > "$out"

click_and_check(){
  local x=$1; local y=$2; local tag=$3
  echo "click $x,$y -> $tag" >> "$out"
  python3 "$VMINPUT" "$Q" click $x $y
  sleep 1
  "$VMSHOT" $tag
  # check if window region vanished by comparing to before: compute diff between tag and before
  magick "$SHOTS/$tag.png" "$before" -compose difference -composite "$SHOTS/check_diff.png"
  # count non-zero pixels
  cnt=$(magick "$SHOTS/check_diff.png" -threshold 5% -format "%@" info: 2>/dev/null || true)
  if [ -z "$cnt" ]; then
    echo "$tag: unable to compute" >> "$out"; return 1
  fi
  # if difference area small, assume closed
  IFS='x+' read W H L T <<< "$cnt" || true
  area=$((W*H))
  echo "$tag geometry $cnt area=$area" >> "$out"
  if [ $area -lt 10000 ]; then
    echo "$tag: closed" >> "$out"; return 0
  else
    echo "$tag: still" >> "$out"; return 1
  fi
}

# test NEW
for name in close max min; do
  x=$((L2 + offs_x[$name])); y=$((T2 + offs_y[$name])); tag=click-new-$name
  if click_and_check $x $y $tag; then
    echo "new $name $x $y closed=YES" >> "$out"
    python3 "$VMINPUT" "$Q" click 440 719; sleep 2; "$VMSHOT" reopened-after-new
  else
    echo "new $name $x $y closed=NO" >> "$out"
  fi
done

# test OLD
for name in close max min; do
  x=$((L1 + offs_x[$name])); y=$((T1 + offs_y[$name])); tag=click-old-$name
  if click_and_check $x $y $tag; then
    echo "old $name $x $y closed=YES" >> "$out"
    python3 "$VMINPUT" "$Q" click 440 719; sleep 2; "$VMSHOT" reopened-after-old
  else
    echo "old $name $x $y closed=NO" >> "$out"
  fi
done

echo DONE >> "$out"
cat "$out"
