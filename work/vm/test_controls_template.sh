#!/bin/bash
set -e
B=/home/jantzen/xxri-build
SHOTS=$B/work/vm/shots
VMINPUT=$B/work/vm-input.py
VMSHOT=$B/work/vm-shot.sh
Q=$B/work/vm/qmp.sock
OUT=$B/work/vm/control-template-results.txt
: > "$OUT"

template_close=$SHOTS/c-close.png
template_max=$SHOTS/c-max.png
template_min=$SHOTS/c-res.png

# images to search
before=$SHOTS/settings-before.png
move=$SHOTS/settings-move-3.png
after=$SHOTS/settings-after.png

find_loc(){
  img=$1; tmpl=$2; lab=$3
  echo "search $lab in $img" >> "$OUT"
  # subimage-search prints "0,0: (x,y)"? we'll use -format "%@[fx:page.x]" maybe simpler: use compare -metric mae ?
  # use -subimage-search
  res=$(magick "$img" "$tmpl" -subimage-search -format "%wx%h+%X+%Y" info:)
  echo "$lab loc: $res" >> "$OUT"
  echo "$res"
}

loc_before_close=$(find_loc "$before" "$template_close" "close_before" ) || true
loc_move_close=$(find_loc "$move" "$template_close" "close_move" ) || true
loc_after_close=$(find_loc "$after" "$template_close" "close_after" ) || true

# parse function to get X,Y of top-left
parse_xy(){ IFS='+' read wh x y <<< "$1"; echo "$x $y"; }
read bx by <<< $(parse_xy "$loc_before_close")
read mx my <<< $(parse_xy "$loc_move_close")
read ax ay <<< $(parse_xy "$loc_after_close")

echo "close coords before:$bx,$by move:$mx,$my after:$ax,$ay" >> "$OUT"

# click at move (visual) then after to test
click_and_record(){
  x=$1; y=$2; tag=$3
  echo "click $tag $x,$y" >> "$OUT"
  python3 "$VMINPUT" "$Q" click $x $y
  sleep 0.6
  "$VMSHOT" "$tag"
  echo "shot $tag saved" >> "$OUT"
}

# Try clicking visual (move/after) coords first
click_and_record $mx $my click-visual-close
# reopen if closed by trying to click dock icon
python3 "$VMINPUT" "$Q" click 440 719 || true; sleep 1
click_and_record $ax $ay click-after-close
# Then click old coords
click_and_record $bx $by click-old-close

# analyze results by checking whether window disappears: compare each shot to before
for t in click-visual-close click-after-close click-old-close; do
  magick "$SHOTS/$t.png" "$before" -compose difference -composite "$SHOTS/diff-$t.png"
  # compute mean pixel value
  m=$(magick "$SHOTS/diff-$t.png" -format "%[fx:mean]" info:)
  echo "$t mean-diff=$m" >> "$OUT"
done

cat "$OUT"
