#!/bin/bash
# build-apps.sh - build the bundled XXRI utility AppImages (Phase 9.1).
#
# Public 32-bit AppImages are essentially extinct, so XXRI Lite ships a small
# set of REAL, functional i686 AppImages of its own.  Each is a genuine type-2
# AppImage (i686 runtime + squashfs payload) with its own icon, name and
# behaviour, so the Store's install/launch/dock/menu/remove flow is proven
# against multiple distinct apps, not one hardcoded sample.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
RUNTIME="$HERE/runtime-i686"
OUT="$HERE/out"; ICONS="$HERE/icons"
FONT=/home/jantzen/xxri-build/rootfs/usr/local/share/fonts/xxri/AvenirNext-Bold.ttf
rm -rf "$OUT" "$ICONS"; mkdir -p "$OUT" "$ICONS"
[ -f "$RUNTIME" ] || { echo "no runtime-i686" >&2; exit 1; }

# make a 256px gradient icon with a big glyph (same look as the Store tiles)
mk_icon() { # id c1 c2 glyph
  magick -size 256x256 "gradient:$2-$3" \
    \( -size 256x256 xc:none -fill white -draw "roundrectangle 14,14,241,241,56,56" \) \
    -compose DstIn -composite \
    -gravity center -pointsize 150 -fill white -font "$FONT" -annotate +0+6 "$4" \
    "$ICONS/$1.png"
}

# assemble one AppImage: the piped body becomes app.sh (its OWN script file, so
# any quoting is safe), and AppRun just runs it inside a titled terminal.
mk_app() { # id "Name" version "Categories"  (app.sh body on stdin)
  local id="$1" name="$2" ver="$3" cats="$4"
  local d="$HERE/pay-$id"; rm -rf "$d"; mkdir -p "$d"
  { echo '#!/bin/sh'; cat; } > "$d/app.sh"; chmod +x "$d/app.sh"
  cat > "$d/AppRun" <<EOF
#!/bin/sh
export DISPLAY="\${DISPLAY:-:0.0}"
DIR="\${APPDIR:-\$(dirname "\$0")}"
exec aterm -T "$name" -e "\$DIR/app.sh"
EOF
  chmod +x "$d/AppRun"
  cp "$ICONS/$id.png" "$d/$id.png"; cp "$ICONS/$id.png" "$d/.DirIcon"
  cat > "$d/$id.desktop" <<EOF
[Desktop Entry]
Name=$name
Exec=AppRun
Icon=$id
Type=Application
Categories=$cats
X-AppImage-Version=$ver
EOF
  mksquashfs "$d" "$HERE/sq-$id.img" -root-owned -noappend -comp gzip -b 131072 >/dev/null 2>&1
  cat "$RUNTIME" "$HERE/sq-$id.img" > "$OUT/$id-$ver.AppImage"
  chmod +x "$OUT/$id-$ver.AppImage"
  rm -rf "$d" "$HERE/sq-$id.img"
  echo "built $OUT/$id-$ver.AppImage ($(wc -c < "$OUT/$id-$ver.AppImage") bytes)"
}

# ---------------------------------------------------------------- the apps ---
mk_icon xxri-clock   "#2E7CF6" "#8371F7" "T"
mk_app xxri-clock "XXRI Clock" 1.0.0 "Utility;Clock;" <<'SH'
clear
while true; do
  printf "\033[H\033[2J\n   XXRI Clock\n   ----------------------------------\n"
  printf "   %s\n\n   (installed from the XXRI Store)\n\n   Ctrl-C to quit.\n" "$(date "+%A %d %B %Y   %H:%M:%S")"
  sleep 1
done
SH

mk_icon xxri-sysinfo "#8371F7" "#ED6CE0" "i"
mk_app xxri-sysinfo "XXRI System Info" 1.0.0 "System;" <<'SH'
echo
echo "   XXRI System Info"
echo "   ================================================"
echo "   Host   : $(hostname 2>/dev/null)"
echo "   Kernel : $(uname -sr 2>/dev/null)"
echo "   Arch   : $(uname -m 2>/dev/null)"
echo "   CPU    : $(awk -F: '/model name/{print $2; exit}' /proc/cpuinfo 2>/dev/null | sed 's/^ *//')"
echo "   Cores  : $(grep -c ^processor /proc/cpuinfo 2>/dev/null)"
echo "   Memory : $(awk '/MemTotal/{printf "%d MiB", $2/1024}' /proc/meminfo 2>/dev/null)"
echo "   Uptime : $(uptime 2>/dev/null | sed 's/^ *//')"
echo "   ------------------------------------------------"
echo "   Installed from the XXRI Store. Close to quit."
echo
exec sh
SH

mk_icon xxri-calc    "#ED6CE0" "#8371F7" "="
mk_app xxri-calc "XXRI Calculator" 1.0.0 "Utility;Calculator;" <<'SH'
echo
echo "   XXRI Calculator  -  type an expression (e.g. 21*2), or q to quit"
echo "   ----------------------------------------------------------------"
while printf "   > " && read expr; do
  [ "$expr" = q ] && break
  [ -z "$expr" ] && continue
  echo "   = $(awk "BEGIN{print $expr}" 2>/dev/null)"
done
SH

mk_icon xxri-notes   "#F06A8E" "#8371F7" "N"
mk_app xxri-notes "XXRI Notes" 1.0.0 "Office;TextEditor;" <<'SH'
NOTE="$HOME/.xxri-notes.txt"
echo
echo "   XXRI Notes  -  a quick scratchpad ($NOTE)"
echo "   -----------------------------------------------------"
[ -f "$NOTE" ] && { echo "   Previous notes:"; sed 's/^/     /' "$NOTE"; echo; }
echo "   Type lines to append; a single . on a line saves and quits."
while printf "   . " && read line; do
  [ "$line" = "." ] && break
  echo "$line" >> "$NOTE"
done
echo "   Saved."
SH

mk_icon xxri-hello   "#34C759" "#2E7CF6" "H"
mk_app xxri-hello "XXRI Hello" 1.0.0 "Utility;" <<'SH'
echo
echo "   ============================================="
echo "     Hello from a real i686 AppImage!"
echo "   ---------------------------------------------"
echo "     Downloaded, SHA256-verified and installed"
echo "     by the XXRI Store through xxri-app."
echo "     PID $$  on $(uname -m)  $(date '+%H:%M:%S')"
echo "   ============================================="
echo
echo "   Close this window to quit."
sleep 3600
SH

echo "--- icons:"; ls "$ICONS"
echo "--- apps:";  ls -l "$OUT"
