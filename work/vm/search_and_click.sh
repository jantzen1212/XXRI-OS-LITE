#!/bin/bash
set -e
B=/home/jantzen/xxri-build
SHOTS=$B/work/vm/shots
VMINPUT=$B/work/vm-input.py
VMSHOT=$B/work/vm-shot.sh
Q=$B/work/vm/qmp.sock
OUT=$B/work/vm/search-click-results.txt
: > "$OUT"

img_before=$SHOTS/settings-before.png
img_after=$SHOTS/settings-after.png
tmpl_close=$SHOTS/c-close.png
tmpl_max=$SHOTS/c-max.png
tmpl_min=$SHOTS/c-res.png

echo "searching templates..." >> "$OUT"

search_template(){
  img=$1; tmpl=$2; step=${3:-4}
  read tw th <<< $(identify -format "%w %h" "$tmpl")
  read iw ih <<< $(identify -format "%w %h" "$img")
  best=999999
  bestx=0
  besty=0
  for ((x=0; x<=iw-tw; x+=step)); do
    for ((y=0; y<=ih-th; y+=step)); do
      magick "$img" -crop ${tw}x${th}+$x+$y +repage m.png
      # compute RMSE
      val=$(magick compare -metric RMSE m.png "$tmpl" null: 2>&1 || true)
      # val like "12345 (0.12345)" extract parenthesized
      if [[ "$val" =~ \(([0-9eE.+-]+)\) ]]; then
        f=${BASH_REMATCH[1]}
      else
        f=999999
      fi
      # compare
      awk -v f="$f" -v best="$best" 'BEGIN{if(f+0 < best+0){print f; exit 0} else {print best; exit 1}}' >/dev/null 2>&1 && {
        best=$f; bestx=$x; besty=$y
      }
    done
  done
  echo "$bestx $besty $best"
}

c_before=($(search_template "$img_before" "$tmpl_close" 6)) || true
c_after=($(search_template "$img_after" "$tmpl_close" 6)) || true
m_before=($(search_template "$img_before" "$tmpl_max" 6)) || true
m_after=($(search_template "$img_after" "$tmpl_max" 6)) || true
r_before=($(search_template "$img_before" "$tmpl_min" 6)) || true
r_after=($(search_template "$img_after" "$tmpl_min" 6)) || true

echo close before: ${c_before[0]},${c_before[1]} after: ${c_after[0]},${c_after[1]} >> "$OUT"
echo max   before: ${m_before[0]},${m_before[1]} after: ${m_after[0]},${m_after[1]} >> "$OUT"
echo min   before: ${r_before[0]},${r_before[1]} after: ${r_after[0]},${r_after[1]} >> "$OUT"

click_and_log(){
  x=$1; y=$2; tag=$3
  echo "click $tag $x,$y" >> "$OUT"
  python3 "$VMINPUT" "$Q" click $x $y
  sleep 0.7
  "$VMSHOT" "$tag"
  echo "shot $tag" >> "$OUT"
}

# click after (visual) controls
click_and_log ${c_after[0]} ${c_after[1]} click-visual-close
click_and_log ${m_after[0]} ${m_after[1]} click-visual-max
click_and_log ${r_after[0]} ${r_after[1]} click-visual-min
# reopen settings to test old coords
python3 "$VMINPUT" "$Q" click 440 719; sleep 1
# click old controls
click_and_log ${c_before[0]} ${c_before[1]} click-old-close
click_and_log ${m_before[0]} ${m_before[1]} click-old-max
click_and_log ${r_before[0]} ${r_before[1]} click-old-min

# output results
cat "$OUT"
