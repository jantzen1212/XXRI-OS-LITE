( LOG="$HOME/rt4.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  say "=== final root-cause probe $(date) ==="

  # ---------- 1. Code OSS: apply the proven fix (libXss.so.1 ENOENT) --------
  say ""
  say "--- Code OSS: libXss.so.1 was NOT FOUND (exit 127). Loading libXss.tcz ---"
  tce-load -wi libXss >/dev/null 2>&1
  say "  libXss.so.1 present now: $(ls /usr/local/lib/libXss.so.1 2>/dev/null || echo NO)"
  B=/opt/xxri/code-oss/VSCode-linux-ia32/code
  say "  ldd NOT FOUND count: $(ldd "$B" 2>&1 | grep -c 'not found')"
  ldd "$B" 2>&1 | grep 'not found' | head -5 | sed 's/^/    /' >> "$LOG"
  ( cd "$HOME"; timeout 30 "$B" --disable-gpu --no-sandbox ) > /tmp/c.out 2>&1
  say "  EXIT CODE: $?"
  head -12 /tmp/c.out | sed 's/^/    /' >> "$LOG"

  # ---------- 2. is libpng itself functional? -------------------------------
  say ""
  say "--- libpng16 integrity (png+jpeg both fail; svg goes through cairo) ---"
  L=$(readlink -f /usr/local/lib/libpng16.so.16)
  say "  path: $L  size=$(wc -c < "$L")"
  say "  ELF : $(hexdump -n 4 -e '4/1 \"%02x\"' "$L")"
  say "  key symbols exported:"
  for s in png_create_read_struct png_read_info png_sig_cmp png_create_info_struct; do
      n=$(nm -D "$L" 2>/dev/null | grep -c " $s\$")
      say "    $s : $n"
  done
  say "  version strings: $(strings "$L" 2>/dev/null | grep -oE '^1\.6\.[0-9]+' | sort -u | tr '\n' ' ')"
  say "  soname: $(readelf -d "$L" 2>/dev/null | grep SONAME | sed 's/.*\[\(.*\)\].*/\1/')"

  # ---------- 3. what does gdk-pixbuf do internally? ------------------------
  say ""
  say "--- gdk-pixbuf: does it dlopen ANY loader module? (svg is a real module) ---"
  S=$(find /usr/local/share/icons -name '*.svg' 2>/dev/null | head -1)
  strace -f -e trace=openat,open -o /tmp/svg.st gdk-pixbuf-thumbnailer "$S" /tmp/s.png >/dev/null 2>&1
  say "  svg decode exit=$?"
  say "  loader .so opens attempted:"
  grep -E 'libpixbufloader' /tmp/svg.st 2>/dev/null | head -6 | sed 's/^/    /' >> "$LOG"
  say "  (none above = the loader list was empty, so nothing was ever tried)"
  say "  cache read:"
  grep -E 'loaders\.cache' /tmp/svg.st 2>/dev/null | head -3 | sed 's/^/    /' >> "$LOG"

  # ---------- 4. glib/gmodule sanity: can anything dlopen at all? -----------
  say ""
  say "--- can glib load modules at all? (gio modules use the same machinery) ---"
  say "  gio modules dir: $(ls /usr/local/lib/gio/modules/ 2>/dev/null | tr '\n' ' ')"
  say "  libgmodule: $(ls /usr/local/lib/libgmodule-2.0.so.0 2>/dev/null || echo MISSING)"

  # ---------- 5. version skew across the glib stack -------------------------
  say ""
  say "--- glib / gdk-pixbuf version skew ---"
  for f in libglib-2.0.so.0 libgobject-2.0.so.0 libgio-2.0.so.0 libgmodule-2.0.so.0 libgdk_pixbuf-2.0.so.0; do
      r=$(readlink -f /usr/local/lib/$f 2>/dev/null)
      say "  $f -> ${r##*/}"
  done
  say "=== RT4 DONE ==="; sync
) >/dev/null 2>&1 &
