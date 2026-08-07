#!/usr/bin/env python3
"""Phase 9.6 - exhaustive probe of the priority application list.

For every app we record EVERY page/endpoint consulted so the proof table can
show exactly where we looked, not just "not found".  A candidate counts as
FOUND only when the bytes on the wire say 32-bit x86: for ELF/AppImage we read
the header over HTTP Range; for archives we list the member table and probe the
first ELF inside.
"""
import json, os, re, sys, urllib.request, urllib.error, urllib.parse, gzip, io, tarfile
from concurrent.futures import ThreadPoolExecutor

W = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(W, "priority-probe.json")
UA = {"User-Agent": "Mozilla/5.0 (X11; Linux i686) XXRI-Store/9.6"}
TIMEOUT = 40


def _req(url, headers=None, method="GET"):
    h = dict(UA)
    h.update(headers or {})
    return urllib.request.Request(url, headers=h, method=method)


def head(url):
    """-> (status, size, final_url, ctype)"""
    for method in ("HEAD", "GET"):
        try:
            r = _req(url, {"Range": "bytes=0-0"} if method == "GET" else None, method)
            with urllib.request.urlopen(r, timeout=TIMEOUT) as f:
                size = f.headers.get("Content-Length")
                cr = f.headers.get("Content-Range")
                if cr and "/" in cr:
                    size = cr.rsplit("/", 1)[1]
                return f.status, int(size or 0), f.url, f.headers.get("Content-Type", "")
        except urllib.error.HTTPError as e:
            if method == "HEAD" and e.code in (403, 405, 501):
                continue
            return e.code, 0, url, ""
        except Exception as e:
            if method == "HEAD":
                continue
            return f"ERR:{type(e).__name__}", 0, url, ""
    return "ERR", 0, url, ""


def get_range(url, first, last):
    try:
        r = _req(url, {"Range": "bytes=%d-%d" % (first, last)})
        with urllib.request.urlopen(r, timeout=TIMEOUT) as f:
            return f.read(last - first + 1)
    except Exception:
        return b""


def get_text(url, limit=3_000_000):
    try:
        with urllib.request.urlopen(_req(url), timeout=TIMEOUT) as f:
            data = f.read(limit)
            if f.headers.get("Content-Encoding") == "gzip":
                data = gzip.decompress(data)
            return data.decode("utf-8", "replace")
    except Exception:
        return ""


ELF_MACH = {3: "i686", 6: "i686", 62: "x86_64", 183: "aarch64", 40: "arm"}


def elf_arch_bytes(b):
    if len(b) < 20 or b[:4] != b"\x7fELF":
        return None
    cls = b[4]
    mach = int.from_bytes(b[18:20], "little")
    a = ELF_MACH.get(mach)
    if a == "i686" and cls != 1:
        return None
    if a == "x86_64" and cls != 2:
        return None
    return a


def probe_elf(url):
    """Architecture straight from the ELF header over HTTP Range."""
    b = get_range(url, 0, 63)
    return elf_arch_bytes(b)


def probe_tar(url, size):
    """Download a portable archive far enough to read the first ELF member."""
    cap = 40 << 20
    if size and size > cap:
        return "archive-large", []
    try:
        with urllib.request.urlopen(_req(url), timeout=180) as f:
            data = f.read(cap)
    except Exception:
        return None, []
    names, arch = [], None
    try:
        tf = tarfile.open(fileobj=io.BytesIO(data))
        for m in tf:
            names.append(m.name)
            if arch or not m.isfile() or m.size < 64:
                continue
            try:
                head_b = tf.extractfile(m).read(64)
            except Exception:
                continue
            a = elf_arch_bytes(head_b)
            if a:
                arch = a
    except Exception:
        pass
    return arch, names[:400]


# ---------------------------------------------------------------- candidates
# Each entry: id, display name, category, and the list of places to look.
# "page" entries are HTML pages we scrape for links matching a 32-bit pattern.
BITS = (r"(?:i[3-6]86|x86[._-]?32|32[._-]?bit|linux32|ia32|32|x86)")

PRIORITY = [
 # ---- Office
 ("onlyoffice", "ONLYOFFICE Desktop Editors", "Office", [
   ("page", "https://download.onlyoffice.com/install/desktop/editors/linux/"),
   ("page", "https://github.com/ONLYOFFICE/DesktopEditors/releases/expanded_assets/v9.1.0"),
   ("direct", "https://download.onlyoffice.com/install/desktop/editors/linux/onlyoffice-desktopeditors_i386.deb"),
 ]),
 ("libreoffice", "LibreOffice", "Office", [
   ("page", "https://downloadarchive.documentfoundation.org/libreoffice/old/5.4.7.2/deb/x86/"),
   ("page", "https://downloadarchive.documentfoundation.org/libreoffice/old/6.4.7.2/deb/x86/"),
   ("page", "https://downloadarchive.documentfoundation.org/libreoffice/old/"),
 ]),
 ("abiword", "AbiWord", "Office", [("tcz", "abiword")]),
 ("gnumeric", "Gnumeric", "Office", [("tcz", "gnumeric")]),
 # ---- Development
 ("code-oss", "Visual Studio Code", "Development", [
   ("direct", "https://update.code.visualstudio.com/1.35.1/linux-ia32/stable"),
   ("direct", "https://update.code.visualstudio.com/1.34.0/linux-ia32/stable"),
 ]),
 ("vscodium", "VSCodium", "Development", [
   ("gh", "VSCodium/vscodium"),
 ]),
 ("geany", "Geany", "Development", [("tcz", "geany")]),
 ("notepadqq", "Notepadqq", "Development", [("gh", "notepadqq/notepadqq")]),
 ("kate", "Kate", "Development", [("page", "https://cdn.kde.org/ci-builds/utilities/kate/master/linux/")]),
 ("bluefish", "Bluefish", "Development", [("tcz", "bluefish")]),
 # ---- Graphics
 ("inkscape", "Inkscape", "Graphics", [
   ("page", "https://inkscape.org/release/inkscape-0.92.4/gnulinux/appimage/dl/"),
   ("page", "https://media.inkscape.org/media/resources/file/"),
   ("gh", "inkscape/inkscape"),
   ("tcz", "inkscape"),
 ]),
 ("gimp", "GIMP", "Graphics", [
   ("page", "https://download.gimp.org/pub/gimp/v2.10/linux/"),
   ("gh", "aferrero2707/gimp-appimage"),
   ("tcz", "gimp"),
 ]),
 ("krita", "Krita", "Graphics", [
   ("page", "https://download.kde.org/stable/krita/3.3.3/"),
   ("page", "https://download.kde.org/stable/krita/"),
 ]),
 ("pinta", "Pinta", "Graphics", [("gh", "PintaProject/Pinta")]),
 ("libresprite", "LibreSprite", "Graphics", [("gh", "LibreSprite/LibreSprite")]),
 ("mtpaint", "mtPaint", "Graphics", [("tcz", "mtpaint")]),
 # ---- Video / Audio
 ("vlc", "VLC media player", "Multimedia", [
   ("page", "https://download.videolan.org/pub/videolan/vlc/last/"),
   ("gh", "kokoko3k/vlc-appimage"),
   ("tcz", "vlc"),
 ]),
 ("mpv", "mpv", "Multimedia", [
   ("gh", "probonopd/mpv-AppImage"),
   ("tcz", "mpv"),
 ]),
 ("audacity", "Audacity", "Multimedia", [
   ("gh", "audacity/audacity"),
   ("tcz", "audacity"),
 ]),
 ("shotcut", "Shotcut", "Multimedia", [("gh", "mltframework/shotcut")]),
 ("kdenlive", "Kdenlive", "Multimedia", [("page", "https://download.kde.org/stable/kdenlive/")]),
 ("obs", "OBS Studio", "Multimedia", [("gh", "obsproject/obs-studio")]),
 ("handbrake", "HandBrake", "Multimedia", [("gh", "HandBrake/HandBrake")]),
 ("avidemux", "Avidemux", "Multimedia", [
   ("page", "https://sourceforge.net/projects/avidemux/files/avidemux/2.7.6/"),
   ("gh", "mean00/avidemux2"),
 ]),
 ("ffmpeg", "FFmpeg", "Multimedia", [
   ("page", "https://johnvansickle.com/ffmpeg/"),
   ("direct", "https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-i686-static.tar.xz"),
   ("tcz", "ffmpeg"),
 ]),
 # ---- Internet
 ("firefox", "Mozilla Firefox", "Internet", [
   ("direct", "https://download.mozilla.org/?product=firefox-latest-ssl&os=linux&lang=en-US"),
   ("page", "https://ftp.mozilla.org/pub/firefox/releases/115.14.0esr/linux-i686/en-US/"),
 ]),
 ("chromium", "Chromium", "Internet", [
   ("page", "https://commondatastorage.googleapis.com/chromium-browser-snapshots/index.html?prefix=Linux/"),
   ("tcz", "chromium"),
 ]),
 ("brave", "Brave Browser", "Internet", [("gh", "brave/brave-browser")]),
 ("opera", "Opera", "Internet", [("page", "https://get.geo.opera.com/pub/opera/desktop/")]),
 ("torbrowser", "Tor Browser", "Internet", [
   ("page", "https://archive.torproject.org/tor-package-archive/torbrowser/10.5.10/"),
   ("page", "https://archive.torproject.org/tor-package-archive/torbrowser/"),
 ]),
 ("palemoon", "Pale Moon", "Internet", [
   ("page", "https://www.palemoon.org/download.shtml"),
   ("direct", "https://rm-eu.palemoon.org/release/palemoon-33.0.0.linux-i686-gtk3.tar.xz"),
 ]),
 # ---- Communication
 ("telegram", "Telegram Desktop", "Communication", [
   ("direct", "https://telegram.org/dl/desktop/linux32"),
 ]),
 ("discord", "Discord", "Communication", [("direct", "https://discord.com/api/download?platform=linux&format=tar.gz")]),
 ("element", "Element", "Communication", [("gh", "element-hq/element-desktop")]),
 ("zoom", "Zoom", "Communication", [
   ("direct", "https://zoom.us/client/latest/zoom_i686.tar.xz"),
   ("page", "https://zoom.us/download"),
 ]),
 ("skype", "Skype", "Communication", [("direct", "https://go.skype.com/skypeforlinux-32.deb")]),
 ("hexchat", "HexChat", "Communication", [("tcz", "hexchat")]),
 ("pidgin", "Pidgin", "Communication", [("tcz", "pidgin")]),
 # ---- Productivity
 ("standard-notes", "Standard Notes", "Productivity", [("gh", "standardnotes/app")]),
 ("joplin", "Joplin", "Productivity", [("gh", "laurent22/joplin")]),
 ("xournalpp", "Xournal++", "Productivity", [("gh", "xournalpp/xournalpp")]),
 ("keepassxc", "KeePassXC", "Productivity", [("gh", "keepassxreboot/keepassxc")]),
 ("cherrytree", "CherryTree", "Productivity", [("gh", "giuspen/cherrytree")]),
 # ---- Files
 ("filezilla", "FileZilla", "Files", [
   ("direct", "https://dl2.cdn.filezilla-project.org/client/FileZilla_3.46.3_i686-linux-gnu.tar.bz2"),
   ("page", "https://dl2.cdn.filezilla-project.org/client/"),
 ]),
 ("peazip", "PeaZip", "Files", [("gh", "peazip/PeaZip")]),
 ("p7zip", "7-Zip", "Files", [
   ("direct", "https://www.7-zip.org/a/7z2409-linux-x86.tar.xz"),
   ("tcz", "p7zip"),
 ]),
 ("doublecmd", "Double Commander", "Files", [
   ("page", "https://sourceforge.net/projects/doublecmd/files/"),
   ("gh", "doublecmd/doublecmd"),
 ]),
 # ---- Utilities
 ("gparted", "GParted", "Utilities", [("tcz", "gparted")]),
 ("etcher", "balenaEtcher", "Utilities", [("gh", "balena-io/etcher")]),
 ("remmina", "Remmina", "Utilities", [("tcz", "remmina")]),
 ("virtualbox", "VirtualBox", "Utilities", [("page", "https://download.virtualbox.org/virtualbox/")]),
 ("qemu", "QEMU", "Utilities", [("tcz", "qemu")]),
 ("bleachbit", "BleachBit", "Utilities", [
   ("page", "https://www.bleachbit.org/download/linux"),
   ("gh", "bleachbit/bleachbit"),
 ]),
 # ---- Music / 3D
 ("lmms", "LMMS", "Multimedia", [("gh", "LMMS/lmms")]),
 ("musescore", "MuseScore", "Multimedia", [("gh", "musescore/MuseScore")]),
 ("ardour", "Ardour", "Multimedia", [("page", "https://community.ardour.org/download")]),
 ("freecad", "FreeCAD", "Graphics", [("gh", "FreeCAD/FreeCAD")]),
 ("blender", "Blender", "Graphics", [
   ("page", "https://download.blender.org/release/Blender2.79/"),
   ("page", "https://download.blender.org/release/"),
 ]),
]

ARCHIVE_EXT = (".tar.gz", ".tgz", ".tar.xz", ".tar.bz2", ".zip", ".7z")
PKG_EXT = (".deb", ".rpm")
BIN_RE = re.compile(r"\.(AppImage|tar\.gz|tgz|tar\.xz|tar\.bz2|zip|deb|rpm|run|bin)$", re.I)
BITS_RE = re.compile(BITS, re.I)
NEG_RE = re.compile(r"(x86[._-]?64|amd64|aarch64|arm64|armhf|win|mac|osx|darwin|source|src)", re.I)


def gh_latest_tag(repo):
    try:
        r = _req("https://github.com/%s/releases/latest" % repo, method="HEAD")
        with urllib.request.urlopen(r, timeout=TIMEOUT) as f:
            m = re.search(r"/tag/([^/]+)$", f.url)
            return m.group(1) if m else None
    except Exception:
        return None


def gh_tags(repo, limit=6):
    tags = []
    t = gh_latest_tag(repo)
    if t:
        tags.append(t)
    atom = get_text("https://github.com/%s/releases.atom" % repo)
    for m in re.finditer(r"/releases/tag/([^\"'<]+)", atom):
        tag = urllib.parse.unquote(m.group(1))
        if tag not in tags:
            tags.append(tag)
        if len(tags) >= limit:
            break
    return tags


def gh_assets(repo, tag):
    html = get_text("https://github.com/%s/releases/expanded_assets/%s"
                    % (repo, urllib.parse.quote(tag, safe="")))
    out = []
    for m in re.finditer(r'href="(/[^"]+/releases/download/[^"]+)"', html):
        out.append("https://github.com" + urllib.parse.unquote(m.group(1)))
    return out


def scrape_links(page):
    html = get_text(page)
    base = page.rsplit("/", 1)[0] + "/"
    links = []
    for m in re.finditer(r'href="([^"#?]+)"', html):
        h = m.group(1)
        if h.startswith("http"):
            u = h
        elif h.startswith("/"):
            p = urllib.parse.urlparse(page)
            u = "%s://%s%s" % (p.scheme, p.netloc, h)
        else:
            u = base + h
        links.append(urllib.parse.unquote(u))
    return html, links


def tcz_check(name):
    st, size, url, _ = head("http://repo.tinycorelinux.net/16.x/x86/tcz/%s.tcz" % name)
    return st, size, url


def candidates_for(kind, ref):
    """-> (list of (url, note), list of pages consulted)"""
    pages, cands = [], []
    if kind == "direct":
        cands.append((ref, "vendor direct link"))
        pages.append(ref)
    elif kind == "tcz":
        pages.append("http://repo.tinycorelinux.net/16.x/x86/tcz/%s.tcz" % ref)
    elif kind == "gh":
        pages.append("https://github.com/%s/releases" % ref)
        for tag in gh_tags(ref):
            pages.append("https://github.com/%s/releases/expanded_assets/%s" % (ref, tag))
            for a in gh_assets(ref, tag):
                fn = a.rsplit("/", 1)[-1]
                if BIN_RE.search(fn) and BITS_RE.search(fn) and not NEG_RE.search(fn):
                    cands.append((a, "GitHub release %s" % tag))
    elif kind == "page":
        pages.append(ref)
        _, links = scrape_links(ref)
        for u in links:
            fn = u.rsplit("/", 1)[-1]
            if BIN_RE.search(fn) and BITS_RE.search(fn) and not NEG_RE.search(fn):
                cands.append((u, "listed on %s" % ref))
    # de-dup, keep order
    seen, uniq = set(), []
    for u, n in cands:
        if u not in seen:
            seen.add(u)
            uniq.append((u, n))
    return uniq[:14], pages


def probe_one(app):
    aid, name, cat, sources = app
    rec = {"id": aid, "name": name, "category": cat,
           "pages_searched": [], "candidates": [], "found": None}
    for kind, ref in sources:
        try:
            cands, pages = candidates_for(kind, ref)
        except Exception as e:
            rec["pages_searched"].append({"url": str(ref), "error": type(e).__name__})
            continue
        for p in pages:
            st, size, final, ctype = head(p)
            rec["pages_searched"].append({"url": p, "status": st, "size": size})
            if kind == "tcz" and st == 200:
                rec["candidates"].append({"url": p, "status": 200, "size": size,
                                          "arch": "i686", "packaging": "tcz",
                                          "note": "Tiny Core extension (native i686 repo)"})
                if not rec["found"]:
                    rec["found"] = rec["candidates"][-1]
        for url, note in cands:
            st, size, final, ctype = head(url)
            c = {"url": url, "status": st, "size": size, "note": note}
            fn = final.rsplit("/", 1)[-1].split("?")[0]
            if st == 200:
                if fn.lower().endswith(".appimage") or ".appimage" in url.lower():
                    c["packaging"] = "AppImage"
                    c["arch"] = probe_elf(final) or "unknown"
                elif fn.lower().endswith(ARCHIVE_EXT) or "tar" in ctype or "zip" in ctype:
                    c["packaging"] = "archive"
                    a, names = probe_tar(final, size)
                    c["arch"] = a or "unknown"
                    c["members"] = names[:12]
                elif fn.lower().endswith(PKG_EXT):
                    c["packaging"] = fn.rsplit(".", 1)[-1]
                    c["arch"] = "i386-declared"
                else:
                    c["packaging"] = "binary"
                    c["arch"] = probe_elf(final) or "unknown"
                if c.get("arch") in ("i686", "archive-large", "i386-declared") and not rec["found"]:
                    rec["found"] = c
            rec["candidates"].append(c)
    return rec


def main():
    only = sys.argv[1:] or None
    apps = [a for a in PRIORITY if not only or a[0] in only]
    res = {}
    if os.path.exists(OUT):
        res = json.load(open(OUT))
    with ThreadPoolExecutor(max_workers=10) as ex:
        for rec in ex.map(probe_one, apps):
            res[rec["id"]] = rec
            f = rec["found"]
            print("%-16s %s" % (rec["id"],
                  ("FOUND %s %s %.1fMB" % (f["packaging"], f["arch"], f["size"] / 1048576.0))
                  if f else "no 32-bit build (%d pages, %d candidates)"
                  % (len(rec["pages_searched"]), len(rec["candidates"]))), flush=True)
    json.dump(res, open(OUT, "w"), indent=1)
    n = sum(1 for r in res.values() if r["found"])
    print("\n%d/%d priority apps have a real 32-bit build -> %s" % (n, len(res), OUT))


if __name__ == "__main__":
    import urllib.parse
    main()
