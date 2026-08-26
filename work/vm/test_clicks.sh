#!/bin/bash
set -e
B=/home/jantzen/xxri-build
SHOTS=$B/work/vm/shots
VMINPUT=$B/work/vm-input.py
VMSHOT=$B/work/vm-shot.sh
Q=$B/work/vm/qmp.sock

trim_geom(){
  local f=$1
  geom=$(magick "$f" -fuzz 12% -trim -format "%@" info:)
  # geom like 800x600+50+20
  echo "$geom"
}

g1=$(trim_geom "$SHOTS/settings-before.png")
g2=$(trim_geom "$SHOTS/settings-after.png")

echo before geom: $g1
echo after  geom: $g2

parse(){
  # input like WxH+L+T
  IFS='x+' read W H L T <<< "$1"
  echo "$L $T $W $H"
}
read L1 T1 W1 H1 <<< $(parse "$g1")
read L2 T2 W2 H2 <<< $(parse "$g2")

echo "tl before: $L1,$T1  after: $L2,$T2  delta: $((L2-L1)),$((T2-T1))"

# control offsets
declare -A offs_x offs_y
offs_x[close]=30; offs_y[close]=18
offs_x[max]=60; offs_y[max]=18
offs_x[min]=90; offs_y[min]=18

out=$B/work/vm/control-click-results.txt
: > "$out"

click_and_check(){
  local x=$1; local y=$2; local tag=$3
  echo "click $x,$y -> $tag" >> "$out"
  python3 "$VMINPUT" "$Q" click $x $y
  sleep 1
  "$VMSHOT" $tag
  # check if window bbox still in shot
  g=$(magick "$SHOTS/$tag.png" -fuzz 12% -trim -format "%@" info:)
  if [ -z "$g" ]; then
    echo "$tag: no bbox" >> "$out"
    return 0
  fi
  # if trimmed bbox area small relative, assume gone
  IFS='x+' read W H L T <<< "$g"
  area=$((W*H))
  echo "$tag geometry $g area=$area" >> "$out"
  if [ $area -lt 10000 ]; then
    echo "$tag: closed" >> "$out"
    return 0
  else
    echo "$tag: still" >> "$out"
    return 1
  fi
}

# test NEW
echo "Testing NEW positions" >> "$out"
for name in close max min; do
  x=$((L2 + offs_x[$name])); y=$((T2 + offs_y[$name])); tag=click-new-$name
  if click_and_check $x $y $tag; then
    echo "new $name $x $y closed=YES" >> "$out"
    # reopen
    python3 "$VMINPUT" "$Q" click 440 719; sleep 2; "$VMSHOT" reopened-after-new
  else
    echo "new $name $x $y closed=NO" >> "$out"
  fi
done

# test OLD
echo "Testing OLD positions" >> "$out"
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
