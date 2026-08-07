#!/usr/bin/env python3
"""Give every curated application a real repository icon.

The Store shows an icon BEFORE anything is installed, so it cannot rely on the
icon xxri-app extracts at install time.  For a native extension the authoritative
icon is inside the .tcz itself (its hicolor theme); for a portable build it is
either inside the archive or, failing that, the project's official logo.

Nothing here invents an image: if no real icon can be obtained the app simply
keeps no repository icon and the Store falls back to a generated tile, which is
exactly the last-resort behaviour the product allows.
"""
import io, os, re, subprocess, sys, tarfile, tempfile, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.dirname(HERE)
ICONS = os.path.join(BUILD, "rootfs/usr/local/share/xxri-store/icons")
CACHE = os.path.join(HERE, "icon-cache")
TCZ = "http://repo.tinycorelinux.net/16.x/x86/tcz"
UA = {"User-Agent": "xxri-store-catalog"}
os.makedirs(ICONS, exist_ok=True)
os.makedirs(CACHE, exist_ok=True)

# id -> tcz name (icon lives in the extension's hicolor theme)
FROM_TCZ = {
    "gimp": "gimp", "inkscape": "inkscape", "geany": "geany",
    "abiword": "abiword", "gnumeric": "gnumeric", "gparted": "gparted",
    "remmina": "remmina", "qemu": "qemu", "mtpaint": "mtpaint",
    "bluefish": "bluefish", "p7zip": "p7zip", "vlc": "vlc",
    "audacity": "audacity",
}

# id -> official project logo (used only when the package ships no icon)
OFFICIAL = {
    "telegram":   "https://raw.githubusercontent.com/telegramdesktop/tdesktop/dev/Telegram/Resources/art/icon256.png",
    "code-oss":   "https://raw.githubusercontent.com/microsoft/vscode/main/resources/linux/code.png",
    "blender":    "https://download.blender.org/branding/icon_512x512.png",
    "torbrowser": "https://raw.githubusercontent.com/TheTorProject/gettorbrowser/master/tor-browser.png",
    "doublecmd":  "https://raw.githubusercontent.com/doublecmd/doublecmd/master/pixmaps/dcicon.png",
}

SIZES = ["256x256", "128x128", "96x96", "64x64", "48x48", "32x32"]


def sh(*a, **kw):
    return subprocess.run(a, capture_output=True, **kw)


def normalise(src_bytes, dst):
    """Write a 128x128 PNG the Store's cairo loader can read."""
    with tempfile.NamedTemporaryFile(delete=False) as t:
        t.write(src_bytes)
        tmp = t.name
    r = sh("magick", tmp, "-resize", "128x128>", "-background", "none",
           "-gravity", "center", "-extent", "128x128", "PNG32:" + dst)
    os.unlink(tmp)
    return r.returncode == 0 and os.path.exists(dst) and os.path.getsize(dst) > 0


def icon_from_tcz(aid, name):
    """Pull the largest hicolor icon out of the extension squashfs."""
    tcz = os.path.join(CACHE, name + ".tcz")
    if not os.path.exists(tcz):
        try:
            with urllib.request.urlopen(
                    urllib.request.Request("%s/%s.tcz" % (TCZ, name), headers=UA),
                    timeout=180) as f:
                open(tcz, "wb").write(f.read())
        except Exception as e:
            return "download failed (%s)" % type(e).__name__
    d = tempfile.mkdtemp()
    try:
        sh("unsquashfs", "-n", "-f", "-d", d, tcz)
        for sz in SIZES:
            appdir = os.path.join(d, "usr/local/share/icons/hicolor", sz, "apps")
            if not os.path.isdir(appdir):
                continue
            names = os.listdir(appdir)
            # exact match first, then reverse-DNS forms such as
            # org.inkscape.Inkscape.png, which is how many projects ship now
            for stem in (name, aid):
                cand = [f for f in names if f.lower() == stem.lower() + ".png"] or \
                       [f for f in names if f.lower().endswith("." + stem.lower() + ".png")]
                if cand:
                    return "ok" if normalise(open(os.path.join(appdir, cand[0]), "rb").read(),
                                             os.path.join(ICONS, aid + ".png")) \
                           else "convert failed"
        for stem in (name, aid):
            p = os.path.join(d, "usr/local/share/pixmaps", stem + ".png")
            if os.path.exists(p):
                return "ok" if normalise(open(p, "rb").read(),
                                         os.path.join(ICONS, aid + ".png")) \
                       else "convert failed"
        return "no icon inside the extension"
    finally:
        sh("rm", "-rf", d)


def icon_from_url(aid, url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=90) as f:
            raw = f.read()
    except Exception as e:
        return "fetch failed (%s)" % type(e).__name__
    return "ok" if normalise(raw, os.path.join(ICONS, aid + ".png")) else "convert failed"


def main():
    only = set(sys.argv[1:])
    todo = []
    for aid, name in FROM_TCZ.items():
        if not only or aid in only:
            todo.append((aid, "tcz", name))
    for aid, url in OFFICIAL.items():
        if not only or aid in only:
            todo.append((aid, "url", url))

    ok = 0
    for aid, kind, ref in todo:
        dst = os.path.join(ICONS, aid + ".png")
        if os.path.exists(dst) and os.path.getsize(dst) > 0:
            print("  %-12s already present" % aid)
            ok += 1
            continue
        res = icon_from_tcz(aid, ref) if kind == "tcz" else icon_from_url(aid, ref)
        print("  %-12s %-4s %s" % (aid, kind, res))
        if res == "ok":
            ok += 1
    print("\n%d/%d curated apps now have a real repository icon" % (ok, len(todo)))


if __name__ == "__main__":
    main()
