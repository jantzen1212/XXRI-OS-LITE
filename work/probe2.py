#!/usr/bin/env python3
"""Second-pass architecture resolution for the priority near-misses.

Pass 1 classified packaging from the *redirected* URL, which CDNs strip of any
extension, so real 32-bit tarballs came back "unknown".  Here we key packaging
off the requested URL, stream just enough of the payload to reach the first ELF
member, and report the architecture that the bytes actually declare.
"""
import io, json, os, re, sys, tarfile, zipfile, lzma, bz2, gzip
import urllib.request, urllib.error, urllib.parse

W = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(W, "priority-probe2.json")
UA = {"User-Agent": "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0",
      "Accept": "*/*", "Accept-Language": "en-US,en;q=0.9"}
ELF_MACH = {3: "i686", 6: "i686", 62: "x86_64", 183: "aarch64", 40: "arm"}


def arch_of(b):
    if len(b) < 20 or b[:4] != b"\x7fELF":
        return None
    a = ELF_MACH.get(int.from_bytes(b[18:20], "little"))
    if a == "i686" and b[4] != 1:
        return None
    if a == "x86_64" and b[4] != 2:
        return None
    return a


def fetch(url, cap, extra=None):
    h = dict(UA)
    h.update(extra or {})
    req = urllib.request.Request(url, headers=h)
    with urllib.request.urlopen(req, timeout=240) as f:
        buf = bytearray()
        while len(buf) < cap:
            chunk = f.read(1 << 20)
            if not chunk:
                break
            buf += chunk
        return bytes(buf), f.url, dict(f.headers)


def scan_tar(data, comp):
    """Walk a (possibly truncated) tar and return the first ELF arch + names."""
    try:
        if comp == "xz":
            d = lzma.LZMADecompressor()
        elif comp == "bz2":
            d = bz2.BZ2Decompressor()
        elif comp == "gz":
            d = None
        else:
            d = None
        if comp == "gz":
            raw = gzip.GzipFile(fileobj=io.BytesIO(data)).read()
        elif d is not None:
            raw = d.decompress(data)
        else:
            raw = data
    except Exception as e:
        try:
            raw = getattr(e, "partial", b"") or b""
        except Exception:
            raw = b""
    if not raw:
        return None, [], 0
    names, arch = [], None
    off = 0
    while off + 512 <= len(raw):
        hdr = raw[off:off + 512]
        if hdr[:1] == b"\0":
            break
        name = hdr[:100].split(b"\0")[0].decode("utf-8", "replace")
        try:
            size = int(hdr[124:136].split(b"\0")[0].strip() or b"0", 8)
        except Exception:
            break
        typ = hdr[156:157]
        body = off + 512
        if name:
            names.append(name)
        if typ in (b"0", b"\0") and size >= 64 and body + 64 <= len(raw) and arch is None:
            a = arch_of(raw[body:body + 64])
            if a:
                arch = a
        off = body + ((size + 511) // 512) * 512
    return arch, names, len(raw)


def scan_zip(data):
    try:
        zf = zipfile.ZipFile(io.BytesIO(data))
    except Exception:
        return None, []
    names = zf.namelist()
    arch = None
    for n in names:
        if arch:
            break
        try:
            with zf.open(n) as fh:
                a = arch_of(fh.read(64))
                if a:
                    arch = a
        except Exception:
            continue
    return arch, names[:15]


def scan_deb(data):
    """ar archive -> data.tar.* -> first ELF."""
    if data[:8] != b"!<arch>\n":
        return None, []
    off = 8
    while off + 60 <= len(data):
        name = data[off:off + 16].decode("ascii", "replace").strip()
        try:
            size = int(data[off + 48:off + 58].decode().strip())
        except Exception:
            return None, []
        body = off + 60
        if name.startswith("data.tar"):
            comp = {"xz": "xz", "gz": "gz", "bz2": "bz2"}.get(name.rsplit(".", 1)[-1].rstrip("/"), "")
            return scan_tar(data[body:body + size], comp)[:2]
        off = body + size + (size & 1)
    return None, []


TARGETS = {
 "onlyoffice":  [("https://github.com/ONLYOFFICE/DesktopEditors/releases/download/v9.1.0/DesktopEditors_x86.zip", 60)],
 "filezilla":   [("https://dl2.cdn.filezilla-project.org/client/FileZilla_3.46.3_i686-linux-gnu.tar.bz2", 40)],
 "doublecmd":   [("https://github.com/doublecmd/doublecmd/releases/download/v1.2.7/doublecmd-1.2.7.gtk2.i386.tar.xz", 40)],
 "skype":       [("https://go.skype.com/skypeforlinux-32.deb", 60)],
 "code-oss":    [("https://update.code.visualstudio.com/1.35.1/linux-ia32/stable", 80)],
 "blender":     [("https://download.blender.org/release/Blender2.79/blender-2.79-linux-glibc219-i686.tar.bz2", 150)],
 "torbrowser":  [("https://archive.torproject.org/tor-package-archive/torbrowser/10.5.10/tor-browser-linux32-10.5.10_en-US.tar.xz", 100)],
 "libreoffice": [("https://downloadarchive.documentfoundation.org/libreoffice/old/5.4.7.2/deb/x86/LibreOffice_5.4.7.2_Linux_x86_deb.tar.gz", 260)],
 "krita":       [("https://download.kde.org/stable/krita/3.3.3/krita-3.3.3-x86.zip", 200)],
 "firefox":     [("https://ftp.mozilla.org/pub/firefox/releases/115.14.0esr/linux-i686/en-US/firefox-115.14.0esr.tar.bz2", 90)],
 "palemoon":    [("https://rm-eu.palemoon.org/release/palemoon-33.4.0.linux-i686-gtk3.tar.xz", 60),
                 ("https://rm-eu.palemoon.org/release/palemoon-32.5.2.linux-i686-gtk3.tar.xz", 60)],
 "zoom":        [("https://zoom.us/client/latest/zoom_i686.tar.xz", 60)],
 "thunderbird": [("https://ftp.mozilla.org/pub/thunderbird/releases/115.14.0/linux-i686/en-US/thunderbird-115.14.0.tar.bz2", 80)],
 "audacity-ai": [("https://github.com/audacity/audacity/releases/download/Audacity-2.4.2/audacity-linux-2.4.2-x86_64.AppImage", 1)],
}


def resolve(url, cap_mb):
    rec = {"url": url}
    fn = urllib.parse.urlparse(url).path.rsplit("/", 1)[-1].lower()
    try:
        data, final, hdrs = fetch(url, cap_mb << 20)
    except urllib.error.HTTPError as e:
        rec["status"] = e.code
        return rec
    except Exception as e:
        rec["status"] = "ERR:%s" % type(e).__name__
        return rec
    rec["status"] = 200
    rec["bytes_read"] = len(data)
    rec["total"] = int(hdrs.get("Content-Length") or 0)
    cd = hdrs.get("Content-Disposition", "")
    m = re.search(r'filename="?([^";]+)', cd)
    if m:
        fn = m.group(1).lower()
        rec["filename"] = m.group(1)
    elif final != url:
        rec["final"] = final
    if fn.endswith(".zip"):
        rec["packaging"], (a, names) = "zip", scan_zip(data)
    elif fn.endswith(".deb"):
        rec["packaging"], (a, names) = "deb", scan_deb(data)
    elif fn.endswith((".tar.xz", ".txz")):
        rec["packaging"] = "tar.xz"
        a, names, _ = scan_tar(data, "xz")
    elif fn.endswith((".tar.bz2", ".tbz")):
        rec["packaging"] = "tar.bz2"
        a, names, _ = scan_tar(data, "bz2")
    elif fn.endswith((".tar.gz", ".tgz")):
        rec["packaging"] = "tar.gz"
        a, names, _ = scan_tar(data, "gz")
    elif data[:4] == b"\x7fELF":
        rec["packaging"] = "AppImage" if data[8:11] == b"AI\x02" else "elf"
        a, names = arch_of(data), []
    else:
        rec["packaging"], a, names = "unknown(%s)" % fn[-12:], None, []
    rec["arch"] = a or "undetermined"
    rec["members"] = names[:10]
    return rec


def main():
    only = sys.argv[1:]
    res = json.load(open(OUT)) if os.path.exists(OUT) else {}
    for aid, urls in TARGETS.items():
        if only and aid not in only:
            continue
        for url, cap in urls:
            r = resolve(url, cap)
            res.setdefault(aid, []).append(r)
            print("%-14s %-6s %-9s %-12s %s" % (
                aid, r.get("status"), r.get("packaging", "-"), r.get("arch", "-"),
                (r.get("members") or [""])[0][:44]), flush=True)
        json.dump(res, open(OUT, "w"), indent=1)
    print("\n-> %s" % OUT)


if __name__ == "__main__":
    main()
