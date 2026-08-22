( LOG="$HOME/rt3.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  P=/usr/local/share/pixmaps/xxri-store.png
  C=/usr/local/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache
  T=gdk-pixbuf-thumbnailer

  say "=== which layer of gdk-pixbuf is broken $(date) ==="
  say "sample: $P ($(wc -c < $P) bytes)"
  say ""

  say "--- A: cache as shipped ---"
  say "  file exists=$([ -f "$C" ] && echo yes || echo no) size=$(wc -c < "$C" 2>/dev/null || echo 0)"
  say "  entries: $(grep -c '^"' "$C" 2>/dev/null)"
  $T "$P" /tmp/a.png >/dev/null 2>&1; say "  decode exit=$?"

  say ""
  say "--- B: no cache at all (built-in loaders only) ---"
  sudo mv "$C" "$C.away" 2>/dev/null
  $T "$P" /tmp/b.png >/dev/null 2>&1; say "  decode exit=$?"
  GDK_PIXBUF_MODULE_FILE=/nonexistent $T "$P" /tmp/b2.png >/dev/null 2>&1
  say "  decode with MODULE_FILE=/nonexistent exit=$?"

  say ""
  say "--- C: hand-written cache declaring png+jpeg as BUILT-IN (empty module path) ---"
  sudo sh -c 'cat > /usr/local/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache' <<"EOC"
# GdkPixbuf Image Loader Modules file
# Automatically generated file, do not edit
#
""
"png" 5 "gdk-pixbuf" "PNG" "LGPL"
"image/png" ""
"png" ""
"\211PNG\r\n\032\n" "" 100

""
"jpeg" 5 "gdk-pixbuf" "JPEG" "LGPL"
"image/jpeg" ""
"jpeg" "jpe" "jpg" ""
"\377\330" "" 100

EOC
  say "  entries now: $(grep -c '^"' "$C")"
  $T "$P" /tmp/c.png >/dev/null 2>&1
  say "  decode exit=$?  out=$(wc -c < /tmp/c.png 2>/dev/null || echo 0) bytes"

  say ""
  say "--- D: same, plus the shipped svg module appended ---"
  sudo sh -c "gdk-pixbuf-query-loaders >> '$C'" 2>/dev/null
  say "  entries now: $(grep -c '^\"' "$C")"
  $T "$P" /tmp/d.png >/dev/null 2>&1
  say "  png decode exit=$?  out=$(wc -c < /tmp/d.png 2>/dev/null || echo 0) bytes"
  S=$(find /usr/local/share/icons -name '*.svg' 2>/dev/null | head -1)
  $T "$S" /tmp/d2.png >/dev/null 2>&1
  say "  svg decode exit=$?  out=$(wc -c < /tmp/d2.png 2>/dev/null || echo 0) bytes"

  say ""
  say "--- E: if C or D worked, does GIMP now start? ---"
  ( cd "$HOME"; timeout 40 /usr/local/bin/gimp ) > /tmp/g.out 2>&1
  say "  gimp exit=$?"
  grep -icE "recognize the image file format" /tmp/g.out 2>/dev/null | sed 's/^/  pixbuf errors: /' >> "$LOG"
  head -6 /tmp/g.out | sed 's/^/    /' >> "$LOG"
  say "=== RT3 DONE ==="; sync
) >/dev/null 2>&1 &
