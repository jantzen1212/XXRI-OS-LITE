#!/usr/bin/env python3
"""discover-i686.py - find AppImages that REALLY run on i686 (Phase 9.2).

Architecture is never guessed from a filename: every candidate asset is probed
with an HTTP Range request and its ELF header is decoded (EI_CLASS + e_machine),
which is the only authoritative answer.  The same probe doubles as dead-link
detection - a URL that 404s, redirects to HTML, or does not start with ELF magic
is rejected, so the catalog can never advertise something that will not install.

Sources, in the order the brief requires:
  * Official GitHub Releases API   (cached under work/gh-cache, 60 req/h budget)
  * Official GitLab Releases API
  * Official developer download pages (pinned URLs, still ELF-probed)
  * AppImageHub metadata           (candidate discovery only)

Output: work/i686-verified.json   consumed by gen-catalog.py.

  ./discover-i686.py            scan the candidate list (uses cache first)
  ./discover-i686.py --budget N cap the number of live GitHub API calls
"""
import json, os, re, sys, time, urllib.request, urllib.error

BUILD = "/home/jantzen/xxri-build"
WORK  = os.path.join(BUILD, "work")
GHC   = os.path.join(WORK, "gh-cache")
FEED  = os.path.join(WORK, "appimagehub-feed.json")
OUT   = os.path.join(WORK, "i686-verified.json")
os.makedirs(GHC, exist_ok=True)

UA = {"User-Agent": "xxri-store-discovery", "Accept": "application/vnd.github+json"}
budget = 60
for i, a in enumerate(sys.argv):
    if a == "--budget" and i + 1 < len(sys.argv): budget = int(sys.argv[i + 1])
spent = 0

# ------------------------------------------------------------------ probing --
EM = {3: "i686", 6: "i686", 62: "x86_64", 183: "aarch64", 40: "arm"}

def elf_arch(url, timeout=45):
    """-> (arch, size) by reading the real ELF header.  (None,0) = unusable."""
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "xxri-store-discovery",
                                                   "Range": "bytes=0-63"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            head = r.read(64)
            cr = r.headers.get("Content-Range") or ""
            size = int(cr.split("/")[-1]) if "/" in cr else int(r.headers.get("Content-Length") or 0)
        if len(head) < 20 or head[:4] != b"\x7fELF":
            return None, 0                      # 404 page, HTML, or not a binary
        cls = head[4]                           # 1 = 32-bit, 2 = 64-bit
        mach = int.from_bytes(head[18:20], "little")
        arch = EM.get(mach)
        if arch == "i686" and cls != 1: arch = None
        return arch, size
    except Exception:
        return None, 0

def gh_release(repo):
    """Latest release JSON, cached.  Rate-limit answers are never cached."""
    global spent
    key = os.path.join(GHC, repo.replace("/", "_") + ".json")
    if os.path.exists(key):
        try: return json.load(open(key))
        except Exception: pass
    if spent >= budget:
        return {"_error": "budget"}
    spent += 1
    try:
        req = urllib.request.Request("https://api.github.com/repos/%s/releases/latest" % repo, headers=UA)
        with urllib.request.urlopen(req, timeout=30) as r:
            j = json.loads(r.read().decode())
        json.dump(j, open(key, "w"))
        return j
    except urllib.error.HTTPError as e:
        if e.code in (403, 429):
            print("   ! rate-limited at %s" % repo, file=sys.stderr)
            return {"_error": "ratelimit"}
        j = {"_error": "HTTP %s" % e.code}
        json.dump(j, open(key, "w"))            # 404 = a real, cacheable answer
        return j
    except Exception as e:
        return {"_error": str(e)}

def gl_release(project):
    """GitLab releases API (project path or numeric id)."""
    global spent
    key = os.path.join(GHC, "gitlab_" + project.replace("/", "_") + ".json")
    if os.path.exists(key):
        try: return json.load(open(key))
        except Exception: pass
    try:
        p = project.replace("/", "%2F")
        req = urllib.request.Request("https://gitlab.com/api/v4/projects/%s/releases" % p,
                                     headers={"User-Agent": "xxri-store-discovery"})
        with urllib.request.urlopen(req, timeout=30) as r:
            j = json.loads(r.read().decode())
        json.dump(j, open(key, "w"))
        return j
    except Exception as e:
        return {"_error": str(e)}

# 32-bit hints in an asset name; assets with NO arch marker are probed too,
# because plenty of older AppImages are i686 without saying so.
HINT32 = re.compile(r"(i[3-6]86|i386|x86(?![_-]?64)|32[-_]?bit|ia32)", re.I)
HINT64 = re.compile(r"(x86[_-]?64|amd64|aarch64|arm64|armhf|armv7|64[-_]?bit)", re.I)
SKIP   = re.compile(r"(zsync|\.sig$|\.asc$|sha256|debug)", re.I)

def candidate_assets(rel):
    """AppImage assets worth probing: 32-bit hinted first, then unmarked ones."""
    if not rel or "_error" in rel or not isinstance(rel, dict): return [], None, None
    tag = (rel.get("tag_name") or "").lstrip("v")
    date = (rel.get("published_at") or "")[:10]
    hinted, plain = [], []
    for a in rel.get("assets") or []:
        n = a.get("name") or ""
        if not n.lower().endswith(".appimage") or SKIP.search(n): continue
        u = a.get("browser_download_url")
        if not u: continue
        if HINT32.search(n):   hinted.append((n, u, a.get("size") or 0))
        elif not HINT64.search(n): plain.append((n, u, a.get("size") or 0))
    return hinted + plain, tag, date

# --------------------------------------------------------------- candidates --
# Repos worth spending API budget on.  Chosen because they are known to publish,
# or historically published, 32-bit builds: the AppImage project's own tools,
# long-lived utilities, editors, terminal tools, emulators and education apps.
CANDIDATES = [
    # --- second sweep: the categories the brief calls out -------------------
    ("Ultimaker/Cura",                    "graphics"),
    ("openscad/openscad",                 "graphics"),
    ("FreeCAD/FreeCAD",                   "science"),
    ("KiCad/kicad-source-mirror",         "science"),
    ("LMMS/lmms",                         "audio"),
    ("Ardour/ardour",                     "audio"),
    ("musescore/MuseScore",               "audio"),
    ("strawberrymusicplayer/strawberry",  "audio"),
    ("tenacityteam/tenacity",             "audio"),
    ("mpv-player/mpv",                    "video"),
    ("mltframework/shotcut",              "video"),
    ("HandBrake/HandBrake",               "video"),
    ("Komet/MediaElch",                   "video"),
    ("smplayer-dev/smplayer",             "video"),
    ("qt/qtbase",                         "development"),
    ("editorconfig/editorconfig",         "development"),
    ("Alexey-T/CudaText",                 "development"),
    ("nasa/openmct",                      "development"),
    ("lapce/lapce",                       "development"),
    ("helix-editor/helix",                "development"),
    ("mawww/kakoune",                     "development"),
    ("xemu-project/xemu",                 "virtualization"),
    ("PCSX2/pcsx2",                       "virtualization"),
    ("dolphin-emu/dolphin",               "virtualization"),
    ("Provenance-Emu/Provenance",         "virtualization"),
    ("citra-emu/citra",                   "virtualization"),
    ("Rosalie241/RMG",                    "virtualization"),
    ("blastrock/pkgj",                    "virtualization"),
    ("fceux/fceux",                       "virtualization"),
    ("bsnes-emu/bsnes",                   "virtualization"),
    ("Mesen-Emu/Mesen",                   "virtualization"),
    ("gnuplot/gnuplot",                   "science"),
    ("scilab/scilab",                     "science"),
    ("veyon/veyon",                       "education"),
    ("kiwix/kiwix-desktop",               "education"),
    ("openboard-org/OpenBoard",           "education"),
    ("Klavaro/klavaro",                   "education"),
    ("celestia/celestia",                 "education"),
    ("openra/OpenRA",                     "games"),
    ("0ad/0ad",                           "games"),
    ("wesnoth/wesnoth",                   "games"),
    ("hedgewars/hedgewars",               "games"),
    ("minetest/minetest",                 "games"),
    ("Anuken/Mindustry",                  "games"),
    ("widelands/widelands",               "games"),
    ("bitwarden/clients",                 "security"),
    ("veracrypt/VeraCrypt",               "security"),
    ("gpg/gnupg",                         "security"),
    ("transmission/transmission",         "networking"),
    ("deluge-torrent/deluge",             "networking"),
    ("filezilla/filezilla",               "networking"),
    ("syncthing/syncthing",               "networking"),
    ("nextcloud/desktop",                 "networking"),
    ("balena-io/etcher",                  "utilities"),
    ("KDE/krusader",                      "system"),
    ("qarmin/czkawka",                    "utilities"),
    ("cyd01/KiTTY",                       "networking"),
    ("Genymobile/scrcpy",                 "utilities"),
    ("ImageMagick/ImageMagick",           "graphics"),
    ("GNOME/gimp",                        "graphics"),
    ("darktable-org/darktable",           "photography"),
    ("Beep6581/RawTherapee",              "photography"),
    ("jgraph/drawio-desktop",             "graphics"),
    ("AppImage/AppImageKit",              "development"),
    ("AppImage/appimaged",                "system"),
    ("AppImageCommunity/pkg2appimage",    "development"),
    ("AppImage/AppImageUpdate",           "system"),
    ("probonopd/linuxdeployqt",           "development"),
    ("standardnotes/desktop",             "productivity"),
    ("qbittorrent/qBittorrent",           "networking"),
    ("audacity/audacity",                 "audio"),
    ("keepassxreboot/keepassxc",          "security"),
    ("Stellarium/stellarium",             "science"),
    ("SuperTux/supertux",                 "games"),
    ("mgba-emu/mgba",                     "virtualization"),
    ("libretro/RetroArch",                "virtualization"),
    ("stenzek/duckstation",               "virtualization"),
    ("TASEmulators/desmume",              "virtualization"),
    ("openMSX/openMSX",                   "virtualization"),
    ("scummvm/scummvm",                   "virtualization"),
    ("dosbox-staging/dosbox-staging",     "virtualization"),
    ("hrydgard/ppsspp",                   "virtualization"),
    ("snes9xgit/snes9x",                  "virtualization"),
    ("visualboyadvance-m/visualboyadvance-m", "virtualization"),
    ("vim/vim-appimage",                  "development"),
    ("neovim/neovim",                     "development"),
    ("zyedidia/micro",                    "development"),
    ("jarun/nnn",                         "system"),
    ("sharkdp/bat",                       "development"),
    ("Peltoche/lsd",                      "system"),
    ("aristocratos/btop",                 "system"),
    ("KDE/kdenlive",                      "video"),
    ("qutebrowser/qutebrowser",           "browser"),
    ("srevinsaju/Firefox-Appimage",       "browser"),
    ("otter-browser/otter-browser",       "browser"),
    ("geogebra/geogebra",                 "education"),
    ("ankitects/anki",                    "education"),
    ("KDE/labplot",                       "science"),
    ("gnucash/gnucash",                   "finance"),
    ("scribusproject/scribus",            "office"),
    ("inkscape/inkscape",                 "graphics"),
    ("nomacs/nomacs",                     "photography"),
    ("BleachBit/bleachbit",               "system"),
    ("peazip/PeaZip",                     "utilities"),
    ("antony-jr/AppImageUpdater",         "system"),
    ("TheAssassin/AppImageLauncher",      "system"),
    ("subsurface/subsurface",             "science"),
    ("cryptomator/cryptomator",           "security"),
    ("FreeTubeApp/FreeTube",              "video"),
    ("laurent22/joplin",                  "productivity"),
    ("marktext/marktext",                 "productivity"),
    ("Zettlr/Zettlr",                     "productivity"),
    ("obsproject/obs-studio",             "video"),
]

# Non-GitHub sources: pinned developer URLs / GitLab projects.  Still ELF-probed.
DIRECT = [
    # id, name, category, url, developer, homepage
]
GITLAB = [
    # (project path, category)
]

# ------------------------------------------------------------------- scan ----
print("scanning %d candidate projects (API budget %d, %d cached)" %
      (len(CANDIDATES), budget, len(os.listdir(GHC))))
found, checked, probed = {}, 0, 0
for repo, cat in CANDIDATES:
    rel = gh_release(repo)
    if rel.get("_error"):
        if rel["_error"] in ("budget", "ratelimit"): print("   … %-42s (no budget/limited)" % repo)
        continue
    checked += 1
    assets, tag, date = candidate_assets(rel)
    if not assets: continue
    hit = None
    for name, url, size in assets[:3]:          # probe at most 3 per project
        probed += 1
        arch, real_size = elf_arch(url)
        if arch == "i686":
            hit = dict(asset=name, url=url, size=real_size or size, tag=tag, date=date)
            break
    if hit:
        found[repo] = dict(category=cat, **hit)
        print("   ✓ %-42s %s  (%s, %.1f MB)" % (repo, hit["asset"], tag, hit["size"] / 1048576))
    else:
        print("   – %-42s no i686 asset" % repo)

for project, cat in GITLAB:
    rel = gl_release(project)
    if isinstance(rel, dict) and rel.get("_error"): continue
    for r in (rel if isinstance(rel, list) else [])[:1]:
        for src in (r.get("assets") or {}).get("links") or []:
            u = src.get("direct_asset_url") or src.get("url") or ""
            if not u.lower().endswith(".appimage"): continue
            probed += 1
            arch, size = elf_arch(u)
            if arch == "i686":
                found["gitlab:" + project] = dict(category=cat, asset=os.path.basename(u),
                                                  url=u, size=size, tag=r.get("tag_name", ""),
                                                  date=(r.get("released_at") or "")[:10])
                print("   ✓ gitlab %-35s %s" % (project, os.path.basename(u)))

for d in DIRECT:
    probed += 1
    arch, size = elf_arch(d["url"])
    if arch == "i686":
        found["direct:" + d["id"]] = dict(category=d["category"], asset=os.path.basename(d["url"]),
                                          url=d["url"], size=size, tag=d.get("version", ""),
                                          date=d.get("date", ""), name=d.get("name"),
                                          developer=d.get("developer"), homepage=d.get("homepage"))
        print("   ✓ direct %-35s ok" % d["id"])

json.dump(found, open(OUT, "w"), indent=1)
print("\nprojects checked: %d   assets ELF-probed: %d   API calls spent: %d" % (checked, probed, spent))
print("VERIFIED i686 AppImages: %d  ->  %s" % (len(found), OUT))
for k, v in found.items():
    print("   %-45s %s" % (k, v["asset"]))
