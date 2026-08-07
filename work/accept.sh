( LOG="$HOME/accept.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  say "=== ACCEPTANCE: fresh image, NO cache command run $(date) ==="
  say ""
  say "--- proof that nothing generated the caches at boot ---"
  PB=/usr/local/lib/gdk-pixbuf-2.0/2.10.0
  say "  loaders.cache      : $(wc -c < $PB/loaders.cache 2>/dev/null || echo MISSING) bytes"
  say "  gschemas.compiled  : $(wc -c < /usr/local/share/glib-2.0/schemas/gschemas.compiled 2>/dev/null || echo MISSING) bytes"
  say "  mime.cache         : $(wc -c < /usr/local/share/mime/mime.cache 2>/dev/null || echo MISSING) bytes"
  say "  Adwaita icon cache : $(wc -c < /usr/local/share/icons/Adwaita/icon-theme.cache 2>/dev/null || echo MISSING) bytes"
  say "  hw-init cache cmds : $(grep -cE 'query-loaders|compile-schemas|gio-querymodules' /etc/init.d/xxri-hw-init 2>/dev/null)"
  say ""
  say "--- STEP 1: gdk-pixbuf-thumbnailer on each format (no setup) ---"
  mkdir -p /tmp/fmt
  cat > /tmp/fmt/t.xpm <<'XPMEOF'
/* XPM */
static char * t_xpm[] = {
"4 4 2 1",
" 	c #FF0000",
".	c #0000FF",
" ...",
". ..",
".. .",
"... "};
XPMEOF
  cp /usr/local/share/pixmaps/xxri-store.png /tmp/fmt/t.png 2>/dev/null
  f=$(find /usr/local/share -name '*.jpg' 2>/dev/null | head -1); [ -n "$f" ] && cp "$f" /tmp/fmt/t.jpg
  f=$(find /usr/local/share -name '*.svg' 2>/dev/null | head -1); [ -n "$f" ] && cp "$f" /tmp/fmt/t.svg
  f=$(find /usr/local/share -name '*.gif' 2>/dev/null | head -1); [ -n "$f" ] && cp "$f" /tmp/fmt/t.gif
  for t in /tmp/fmt/t.*; do
      [ -f "$t" ] || continue
      o="/tmp/fmt/o_$(basename $t).png"
      gdk-pixbuf-thumbnailer "$t" "$o" >/tmp/fmt/e 2>&1
      rc=$?
      say "  $(basename $t): exit=$rc out=$(wc -c < "$o" 2>/dev/null || echo 0) B $(grep -o 'Couldn.t recognize' /tmp/fmt/e | head -1)"
  done
  say ""
  say "--- loaders the runtime knows about ---"
  say "  $(grep -cE '^\"[a-z0-9]+\" [0-9]+ \"gdk-pixbuf\"' $PB/loaders.cache 2>/dev/null) formats in the shipped cache"
  sync
) >/dev/null 2>&1 &
