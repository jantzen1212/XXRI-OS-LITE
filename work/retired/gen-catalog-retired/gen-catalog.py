#!/usr/bin/env python3
# gen-catalog.py - build the bundled XXRI Store repository (Phase 9).
#
# The bundled repository is the OFFLINE fallback shipped inside the ISO; the
# very same layout is what a remote repository serves, so the Store code never
# changes when the catalog grows or moves.
#
# Produces, under rootfs/usr/local/share/xxri-store/:
#   repository/meta.json         format version + counts (future format bumps)
#   repository/<category>.json   rich per-category catalog (parsed by the GUI)
#   repository/index.tsv         flat index (parsed by the busybox backend)
#   repository/sections.json     home-page section -> [app ids]
#   icons/<id>.png               real 128px icon for the curated top apps
#
# Data sources (never hand-typed URLs, never scraped HTML):
#   * AppImageHub feed.json  - metadata only (name/desc/category/author/icon)
#   * GitHub Release API     - real assets/versions/dates for the curated top
#   * bundled ISO payload    - the offline sample app
#
# Inputs are cached under work/ so re-runs are offline and reproducible.
import json, os, re, sys, html, hashlib, subprocess, urllib.request, urllib.error, time

HERE  = os.path.dirname(os.path.abspath(__file__))
BUILD = "/home/jantzen/xxri-build"
ROOT  = os.path.join(BUILD, "rootfs", "usr", "local", "share", "xxri-store")
REPO  = os.path.join(ROOT, "repository")
ICONS = os.path.join(ROOT, "icons")
WORK  = os.path.join(BUILD, "work")
FEED  = os.path.join(WORK, "appimagehub-feed.json")
GHC   = os.path.join(WORK, "gh-cache")          # cached GitHub API responses
HUBDB = "https://appimage.github.io/database/"
os.makedirs(REPO, exist_ok=True); os.makedirs(ICONS, exist_ok=True); os.makedirs(GHC, exist_ok=True)

FMT_VERSION = 1          # repository format version - the Store checks this

# ---------------------------------------------------------------- categories --
CATS = [
 ("browser","Browsers"),("office","Office"),("development","Development"),
 ("graphics","Graphics"),("photography","Photography"),("video","Video"),
 ("audio","Audio"),("education","Education"),("communication","Communication"),
 ("ai","AI"),("games","Games"),("utilities","Utilities"),("science","Science"),
 ("finance","Finance"),("security","Security"),("system","System"),
 ("virtualization","Virtualization"),("networking","Networking"),
 ("productivity","Productivity"),("accessories","Accessories"),
]
CAT_IDS = [c for c, _ in CATS]

# freedesktop category -> xxri category (first match wins, in this order)
FD_MAP = [
 ("WebBrowser","browser"),
 ("IDE","development"),("Development","development"),("WebDevelopment","development"),
 ("Database","development"),("Debugger","development"),("RevisionControl","development"),
 ("Photography","photography"),("Scanning","photography"),
 ("VectorGraphics","graphics"),("RasterGraphics","graphics"),("3DGraphics","graphics"),("Graphics","graphics"),
 ("Video","video"),("VideoConference","communication"),
 ("Music","audio"),("Audio","audio"),("Sequencer","audio"),("Midi","audio"),
 ("InstantMessaging","communication"),("Chat","communication"),("IRCClient","communication"),
 ("Email","communication"),("Telephony","communication"),("News","communication"),
 ("Game","games"),("ActionGame","games"),("AdventureGame","games"),("ArcadeGame","games"),
 ("BoardGame","games"),("LogicGame","games"),("RolePlaying","games"),("Simulation","games"),
 ("StrategyGame","games"),("SportsGame","games"),("CardGame","games"),("KidsGame","games"),
 ("Emulator","virtualization"),
 ("Finance","finance"),
 ("Security","security"),
 ("Astronomy","science"),("Physics","science"),("Chemistry","science"),("Biology","science"),
 ("Math","science"),("NumericalAnalysis","science"),("Engineering","science"),
 ("Electronics","science"),("HamRadio","science"),("Geoscience","science"),("Science","science"),
 ("Education","education"),("Languages","education"),
 ("WordProcessor","office"),("Spreadsheet","office"),("Presentation","office"),
 ("Calendar","productivity"),("ProjectManagement","productivity"),("ContactManagement","productivity"),
 ("Office","office"),
 ("P2P","networking"),("FileTransfer","networking"),("RemoteAccess","networking"),
 ("Monitor","system"),("Network","networking"),
 ("TerminalEmulator","system"),("Filesystem","system"),("FileManager","system"),
 ("Settings","system"),("HardwareSettings","system"),("PackageManager","system"),
 ("System","system"),
 ("Archiving","utilities"),("Compression","utilities"),("TextEditor","accessories"),
 ("Calculator","accessories"),("Clock","accessories"),("Accessibility","accessories"),
 ("Utility","utilities"),
 ("AudioVideo","video"),
]
IGNORE_FD = {"Qt","GTK","GNOME","KDE","Application","Java","Motif","XFCE","Player","Viewer","Building","ConsoleOnly"}
GENERIC_FD = {"Utility"}          # too vague to route on - ask the keywords first

# Topical keyword routing.  Ordered: the first hit wins.  This is what keeps
# "Utility" (a third of the feed) from becoming a dumping ground.
KEYWORD_CATS = [
 ("ai",            r"\b(ai|a\.i\.|llm|llms|gpt|chatgpt|openai|stable[- ]diffusion|whisper|ollama|"
                   r"machine learning|neural|transformer|copilot|langchain|deep ?learning)\b"),
 ("virtualization",r"\b(virtual machine|virtualbox|qemu|docker|container|hypervisor|emulator|emulation)\b"),
 ("security",      r"\b(password manager|passwords|encrypt|encryption|vault|2fa|totp|otp|keepass|vpn|"
                   r"gpg|pgp|cipher|antivirus|firewall|privacy)\b"),
 ("browser",       r"\b(web ?browser|browse the web)\b"),
 ("photography",   r"\b(photo|photos|photograph|raw (image|file|photo)|exif|lightroom|darkroom|"
                   r"image viewer|gallery|camera)\b"),
 ("communication", r"\b(chat|messenger|messaging|matrix client|irc|e-?mail|mail client|slack|discord|"
                   r"telegram|whatsapp|video call|conferenc)\w*\b"),
 ("productivity",  r"\b(to-?do|todo|task manager|tasks|kanban|note-?tak|notes|calendar|planner|"
                   r"pomodoro|time track|knowledge base|outliner|markdown editor|mind ?map)\b"),
 ("finance",       r"\b(accounting|budget|invoice|expense|crypto ?currency|bitcoin|wallet|banking|"
                   r"portfolio|stocks|trading)\b"),
 ("audio",         r"\b(music player|audio (editor|player|record)|podcast|mp3|daw|synthes|mixer|"
                   r"sound editor|spotify)\b"),
 ("video",         r"\b(video (player|editor|convert|downloader)|movie|youtube|streaming|subtitle|"
                   r"screen record|transcod)\w*\b"),
 ("graphics",      r"\b(vector graphic|image editor|drawing|paint|3d model|diagram|design tool|"
                   r"illustrat|render)\w*\b"),
 ("games",         r"\b(game|arcade|puzzle|rpg|roguelike|platformer|shooter|chess|sudoku)\b"),
 ("development",   r"\b(ide|code editor|compiler|debugger|git client|api client|sql|database|"
                   r"regex|json editor|terminal emulator|programming|developer tool)\b"),
 ("science",       r"\b(astronom|physics|chemistry|biolog|mathematic|equation|simulat|cad |"
                   r"circuit|pcb|engineering|statistic)\w*\b"),
 ("networking",    r"\b(torrent|ftp|ssh|remote desktop|download manager|network|proxy|dns|server|"
                   r"cloud sync|file sync)\b"),
 ("office",        r"\b(office suite|word processor|spreadsheet|presentation|pdf|document|ebook|epub)\b"),
 ("system",        r"\b(system (monitor|info)|disk usage|partition|process|hardware info|backup|"
                   r"cleaner|package manager|boot)\b"),
 ("accessories",   r"\b(calculator|clipboard|clock|timer|stopwatch|color picker|screenshot|emoji|"
                   r"launcher|weather|unit convert|text editor)\b"),
 ("education",     r"\b(learn|flashcard|study|typing tutor|language|dictionary|course)\b"),
]
KEYWORD_CATS = [(c, re.compile(rx, re.I)) for c, rx in KEYWORD_CATS]

def keyword_category(blob):
    for c, rx in KEYWORD_CATS:
        if rx.search(blob): return c
    return None

# A browser is tagged "Network" by freedesktop, which would file Firefox under
# Networking.  Known browser engines/forks are matched on the NAME.
BROWSER_NAME_RE = re.compile(
    r"^(firefox|chromium|chrome|brave|vivaldi|opera|waterfox|librewolf|palemoon|pale.?moon|"
    r"midori|falkon|qutebrowser|epiphany|thorium|floorp|zen.?browser|tor.?browser|ungoogled.*|"
    r"min|otter.?browser|catalyst.?browser|monot|nyxt|dot.?browser|badwolf)(.appimage|.browser)?$", re.I)

# Editorial routing for the curated top apps: where the feed's freedesktop tags
# disagree with where a user would look for the app, the curator wins.
FORCE_CAT = {
 "firefox":"browser","firefox_beta":"browser","firefox_esr":"browser","firefox_nightly":"browser",
 "thunderbird":"communication","thunderbird_beta":"communication","thunderbird_nightly":"communication",
 "element":"communication","franz":"communication",
 "vlc":"video","freetube":"video","mediaelch":"video","shotcut":"video",
 "audacity":"audio","lmms":"audio","musescore":"audio",
 "joplin":"productivity","obsidian":"productivity","qownnotes":"productivity","standard_notes":"productivity",
 "keepassxc":"security","bitwarden":"security","cryptomator":"security",
 "stellarium":"science","freecad2":"science","kicad":"science","librepcb":"science",
 "cutter":"development","gvim":"development","neovim":"development",
 "cpu-x":"system","etcher":"utilities",
 "rawtherapee":"photography","digikam":"photography",
 "libreofficefresh":"office",
 "qbittorrent_enhanced_edition":"networking",
}

def pick_category(item, headline):
    name = item.get("name") or ""
    forced = FORCE_CAT.get(name.lower())
    if forced: return forced
    if BROWSER_NAME_RE.match(name.strip()): return "browser"
    """headline = name + the first sentence.  Routing on the headline instead of
    the whole description keeps a cloud-gaming server out of Browsers just
    because paragraph four mentions a web browser."""
    fds  = [c for c in (item.get("categories") or []) if c and c not in IGNORE_FD]
    specific = [c for c in fds if c not in GENERIC_FD]
    # overrides the freedesktop set cannot express (an "AI chat" is tagged Utility)
    for c in ("ai", "virtualization", "security"):
        rx = dict(KEYWORD_CATS)[c]
        if rx.search(headline) and "Game" not in fds: return c
    # nothing but "Utility"? let the topic decide before falling back
    if not specific:
        k = keyword_category(headline)
        if k: return k
    for fd, x in FD_MAP:
        if fd in fds: return x
    return keyword_category(headline) or "utilities"

# ------------------------------------------------------------------ curated --
# The "top apps" - ordering here is the Store's editorial ranking.  Each entry
# is matched against the AppImageHub feed by name; unmatched names are dropped
# (we never invent a download that does not exist).
#   name-in-feed : (tags, nice display name or None)
CURATED = [
 ("Firefox",                    ("featured","trending","recommended"), "Firefox"),
 ("LibreOfficeFresh",           ("featured","trending","recommended"), "LibreOffice Fresh"),
 ("Joplin",                     ("featured","recommended","newest"),   "Joplin"),
 ("element",                    ("featured","recommended"),            "Element"),
 ("KeePassXC",                  ("featured","recommended","editors"),  "KeePassXC"),
 ("VLC",                        ("featured","trending","recommended"), "VLC"),
 ("Audacity",                   ("featured","recommended","editors"),  "Audacity"),
 ("Obsidian",                   ("trending","recommended"),            "Obsidian"),
 ("Thunderbird",                ("recommended",),                      "Thunderbird"),
 ("Bitwarden",                  ("trending","recommended"),            "Bitwarden"),
 ("Standard_Notes",             ("recommended",),                      "Standard Notes"),
 ("FreeTube",                   ("trending","newest"),                 "FreeTube"),
 ("Stellarium",                 ("recommended",),                      "Stellarium"),
 ("neovim",                     ("trending","editors"),                "Neovim"),
 ("Shotcut",                    ("editors","recommended"),             "Shotcut"),
 ("CPU-X",                      ("recommended",),                      "CPU-X"),
 ("Cryptomator",                ("recommended",),                      "Cryptomator"),
 ("MuseScore",                  ("recommended",),                      "MuseScore"),
 ("LMMS",                       ("recommended",),                      "LMMS"),
 ("RawTherapee",                ("editors",),                          "RawTherapee"),
 ("Cutter",                     ("editors",),                          "Cutter"),
 ("FreeCAD2",                   ("trending",),                         "FreeCAD"),
 ("KiCad",                      (),                                    "KiCad"),
 ("MediaElch",                  (),                                    "MediaElch"),
 ("Franz",                      (),                                    "Franz"),
 ("GVim",                       ("editors",),                          "GVim"),
 ("qBittorrent_Enhanced_Edition",("recommended",),                     "qBittorrent Enhanced"),
 ("QOwnNotes",                  (),                                    "QOwnNotes"),
 ("Etcher",                     ("recommended",),                      "balenaEtcher"),
 ("LibrePCB",                   (),                                    "LibrePCB"),
]
CURATED_ORDER = {c[0].lower(): i for i, c in enumerate(CURATED)}
CURATED_TAGS  = {c[0].lower(): c[1] for c in CURATED}
CURATED_NAME  = {c[0].lower(): c[2] for c in CURATED}

# ------------------------------------------------------------------- helpers --
def pretty(name):
    return name.replace("_", " ").strip()

# A fifth of the feed writes its description in HTML.  The Store renders plain
# text, so markup is stripped here, at catalog build time, once.
TAG_RE = re.compile(r"<[^>]+>")
# Emoji, dingbats and symbols in a blurb are noise in a store listing (and used
# to leak as "u2714ufe0f" through the flat-JSON parser); strip them.
SYMBOL_RE = re.compile(
    "[" "\U0001F000-\U0001FAFF" "\U00002190-\U000021FF" "\U00002300-\U000027BF"
    "\U00002B00-\U00002BFF" "\U0000FE00-\U0000FE0F" "\U00002000-\U0000206F"
    "\U0000200D\U000024C2" "]+", flags=re.UNICODE)
def clean_html(s):
    if not s: return ""
    s = re.sub(r"<(br|/p|/li|/div)[^>]*>", " ", s, flags=re.I)
    s = re.sub(r"<li[^>]*>", " - ", s, flags=re.I)
    s = TAG_RE.sub("", s)
    s = html.unescape(s)
    s = SYMBOL_RE.sub(" ", s)
    return re.sub(r"\s+", " ", s).strip(" -–—")

def first_sentence(s, limit=170):
    s = s.strip()
    m = re.search(r"(?<=[.!?])\s", s[:limit+40])
    out = s[:m.start()] if m and m.start() <= limit else s[:limit]
    return out.strip()

def ellipsize(s, n):
    s = s.strip()
    if len(s) <= n: return s
    cut = s[:n].rsplit(" ", 1)[0]
    return (cut or s[:n]).rstrip(" ,.;:-") + "…"

def slug(name):
    s = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return s or "app"

def fetch(url, dest=None, timeout=25, binary=False):
    req = urllib.request.Request(url, headers={"User-Agent":"xxri-store-catalog",
                                               "Accept":"application/vnd.github+json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        data = r.read()
    if dest:
        with open(dest, "wb") as f: f.write(data)
    return data if binary else data.decode("utf-8", "replace")

def gh_release(repo):
    """Latest release JSON for owner/repo, cached on disk (API budget is 60/h).
    A 404 (no releases) is a real answer and is cached; a 403/429 is a rate-limit
    and is NOT cached, so a later run picks the repo up."""
    key = os.path.join(GHC, repo.replace("/", "_") + ".json")
    if os.path.exists(key):
        try: return json.load(open(key))
        except Exception: pass
    try:
        j = json.loads(fetch("https://api.github.com/repos/%s/releases/latest" % repo))
        with open(key, "w") as f: json.dump(j, f)
        return j
    except urllib.error.HTTPError as e:
        j = {"_error": "HTTP %s" % e.code}
        if e.code in (403, 429):
            print("   ! rate-limited on %s (not cached; re-run later)" % repo, file=sys.stderr)
            return j
        with open(key, "w") as f: json.dump(j, f)
        return j
    except Exception as e:
        return {"_error": str(e)}

def verify_direct(url):
    """A catalog URL must serve a real AppImage, not a download page.  Read the
    first bytes and demand ELF magic.  -> (ok, size)"""
    try:
        req = urllib.request.Request(url, headers={"User-Agent":"xxri-store-catalog",
                                                   "Range":"bytes=0-63"})
        with urllib.request.urlopen(req, timeout=40) as r:
            head = r.read(64)
            cr = r.headers.get("Content-Range") or ""
            size = int(cr.split("/")[-1]) if "/" in cr else int(r.headers.get("Content-Length") or 0)
        return head.startswith(b"\x7fELF"), size
    except Exception:
        return False, 0

ARCH_PATTERNS = [
 ("aarch64", re.compile(r"(aarch64|arm64)", re.I)),
 ("arm",     re.compile(r"(armhf|armv7|arm32)", re.I)),
 ("x86_64",  re.compile(r"(x86[_-]?64|amd64|64bit|64-bit)", re.I)),
 ("i686",    re.compile(r"(i[3-6]86|x86(?![_-]?64)|32bit|32-bit)", re.I)),
]
def asset_arch(name):
    for a, rx in ARCH_PATTERNS:
        if rx.search(name): return a
    return None

def release_assets(rel):
    """-> {arch: {url,size,sha256}}, version, date  from a GitHub release."""
    out = {}
    if not rel or "_error" in rel: return out, None, None
    ver = (rel.get("tag_name") or "").lstrip("v")
    date = (rel.get("published_at") or "")[:10]
    for a in rel.get("assets") or []:
        n = a.get("name") or ""
        if not n.lower().endswith(".appimage"): continue
        if re.search(r"(zsync|\.sig|\.asc|debug)", n, re.I): continue
        ar = asset_arch(n) or "x86_64"
        if ar in out: continue                      # first (best) asset per arch wins
        dig = a.get("digest") or ""
        sha = dig.split("sha256:")[-1] if dig.startswith("sha256:") else "-"
        out[ar] = {"url": a.get("browser_download_url"), "size": a.get("size") or 0, "sha256": sha}
    return out, ver, date

def sha256_of_url(url, max_bytes=12*1024*1024):
    """Pin a checksum for small assets we can afford to hash at build time."""
    try:
        req = urllib.request.Request(url, headers={"User-Agent":"xxri-store-catalog"})
        h = hashlib.sha256(); n = 0
        with urllib.request.urlopen(req, timeout=90) as r:
            while True:
                b = r.read(65536)
                if not b: break
                n += len(b)
                if n > max_bytes: return None, 0
                h.update(b)
        return h.hexdigest(), n
    except Exception:
        return None, 0

# --------------------------------------------------------------- build apps --
feed = json.load(open(FEED))["items"]
apps = {}
skipped = 0
for it in feed:
    name = it.get("name") or ""
    desc = clean_html(it.get("description") or "")
    links = {l.get("type"): l.get("url") for l in (it.get("links") or [])}
    gh = links.get("GitHub")
    dl = links.get("Download") or ""
    key = name.lower()
    curated = key in CURATED_ORDER
    if not name:
        skipped += 1; continue
    if not gh and not dl.lower().endswith(".appimage"):
        skipped += 1; continue                       # no way to ever install it
    if not desc:
        # a curated top app is worth keeping even when the feed omits its blurb
        if not curated: skipped += 1; continue
        desc = "%s for XXRI OS Lite." % (CURATED_NAME.get(key) or pretty(name))
    disp = CURATED_NAME.get(key) or pretty(name)
    aid = slug(name)
    if aid in apps: continue
    headline = disp + ". " + first_sentence(desc)
    cat = pick_category(it, headline)
    authors = it.get("authors") or []
    dev = (authors[0].get("name") if authors else "") or (gh.split("/")[0] if gh else "Unknown")
    homepage = (authors[0].get("url") if authors else "") or (("https://github.com/" + gh) if gh else dl)
    icons = it.get("icons") or []
    icon_url = (HUBDB + icons[0]) if icons else ""
    shots = [HUBDB + s for s in (it.get("screenshots") or [])][:3]
    tags = list(CURATED_TAGS.get(key, ()))
    rank = 0
    if key in CURATED_ORDER: rank = 1000 - CURATED_ORDER[key]
    else:
        if shots: rank += 10
        if it.get("license"): rank += 5
        if len(desc) > 30: rank += 5
        if gh: rank += 3
    apps[aid] = dict(
        id=aid, name=disp, category=cat, kind="appimage",
        summary=ellipsize(first_sentence(desc, 110), 110), description=desc,
        developer=dev, publisher=dev,
        license=it.get("license") or "Open Source",
        homepage=homepage, website=homepage,
        arch=["x86_64"],                     # refined by the enrichment pass
        version="latest", release_date="", updated="",
        size=0, download_size=0, sha256="-",
        source_type=("github" if gh else "direct"),
        source_ref=((gh + " *.AppImage") if gh else ""),
        url=("" if gh else dl),
        assets={}, icon_url=icon_url, screenshots=shots,
        permissions=["Network access","Local files","Desktop integration"],
        changelog="", dependencies=[], tags=tags, rank=rank,
        # Part 8: nothing is "verified" until a concrete release asset has been
        # confirmed for it.  Feed metadata alone never earns that flag.
        verified=False,
    )

# ------------------------------------------------------- curated extras ------
# Household apps AppImageHub has no link for.  Each one is VERIFIED at build
# time (GitHub Release API, or ELF magic on a direct URL) and silently dropped
# if it does not resolve - the catalog never ships a download that isn't real.
#   id, name, category, summary, developer, license, homepage, tags, source
EXTRAS = [
 ("onlyoffice","ONLYOFFICE Desktop","office","Full office suite compatible with Microsoft formats",
  "Ascensio System SIA","AGPL-3.0","https://www.onlyoffice.com",("featured","recommended","editors"),
  ("github","ONLYOFFICE/DesktopEditors")),
 ("vscodium","VSCodium","development","Community build of VS Code without telemetry",
  "VSCodium","MIT","https://vscodium.com",("featured","recommended","editors"),
  ("github","VSCodium/vscodium")),
 ("logseq","Logseq","productivity","Privacy-first outliner and knowledge base",
  "Logseq","AGPL-3.0","https://logseq.com",("trending","newest"),
  ("github","logseq/logseq")),
 ("drawio","draw.io","graphics","Diagrams, flowcharts and whiteboards, offline",
  "JGraph","Apache-2.0","https://www.drawio.com",("recommended",),
  ("github","jgraph/drawio-desktop")),
 ("jan","Jan","ai","Run open large language models offline on your own computer",
  "Menlo Research","AGPL-3.0","https://jan.ai",("featured","newest","trending"),
  ("github","janhq/jan")),
 ("krita","Krita","graphics","Professional digital painting studio",
  "KDE","GPL-3.0","https://krita.org",("featured","trending","editors"),
  ("direct","https://download.kde.org/stable/krita/5.2.9/krita-5.2.9-x86_64.AppImage","5.2.9")),
 ("kdenlive","Kdenlive","video","Powerful multi-track video editor",
  "KDE","GPL-3.0","https://kdenlive.org",("trending","editors"),
  ("direct","https://download.kde.org/stable/kdenlive/24.12/linux/kdenlive-24.12.3-x86_64.AppImage","24.12.3")),
 ("libreoffice","LibreOffice","office","The free and open source office suite",
  "The Document Foundation","MPL-2.0","https://www.libreoffice.org",("trending",),
  ("direct","https://appimages.libreitalia.org/LibreOffice-fresh.basic-x86_64.AppImage","fresh")),
]
extras_ok = extras_dropped = 0
for exi, (eid, ename, ecat, esum, edev, elic, ehome, etags, src) in enumerate(EXTRAS):
    entry = dict(id=eid, name=ename, category=ecat, kind="appimage",
        summary=esum, description=esum + ".", developer=edev, publisher=edev,
        license=elic, homepage=ehome, website=ehome, arch=["x86_64"],
        version="latest", release_date="", updated="", size=0, download_size=0,
        sha256="-", source_type=src[0], source_ref="", url="", assets={},
        icon_url="", screenshots=[],
        permissions=["Network access","Local files","Desktop integration"],
        changelog="", dependencies=[], tags=list(etags), rank=998 - exi,
        verified=True)
    if src[0] == "github":
        rel = gh_release(src[1])
        assets, ver, date = release_assets(rel)
        if not assets:
            print("   - dropped %s (no AppImage asset in the latest release)" % eid); extras_dropped += 1; continue
        entry.update(source_ref=src[1] + " *.AppImage", assets=assets, arch=sorted(assets),
                     version=ver or "latest", release_date=date or "", updated=date or "")
        big = assets.get("x86_64") or list(assets.values())[0]
        entry.update(url=big["url"], size=big["size"], download_size=big["size"],
                     sha256=big.get("sha256") or "-")
        body = (rel.get("body") or "").strip()
        if body: entry["changelog"] = body[:600]
    else:
        ok, size = verify_direct(src[1])
        if not ok:
            print("   - dropped %s (URL does not serve an AppImage)" % eid); extras_dropped += 1; continue
        entry.update(url=src[1], size=size, download_size=size, version=src[2],
                     assets={"x86_64": {"url": src[1], "size": size, "sha256": "-"}})
    apps[eid] = entry; extras_ok += 1

# ------------------------------------------------- enrich the curated top ----
# Real versions / dates / per-arch assets from the GitHub Release API.  This is
# what lets the Store pin a SHA256 and know an app's true architectures.
enriched = 0
slug2curated = {slug(k): k for k in CURATED_ORDER}
for aid, a in list(apps.items()):
    if aid not in slug2curated or a["source_type"] != "github": continue
    rel = gh_release(a["source_ref"].split()[0])
    assets, ver, date = release_assets(rel)
    if not assets: continue
    a["assets"] = assets
    a["arch"] = sorted(assets.keys())
    if ver:  a["version"] = ver
    if date: a["release_date"] = a["updated"] = date
    body = (rel.get("body") or "").strip()
    if body: a["changelog"] = body[:600]
    big = assets.get("x86_64") or list(assets.values())[0]
    a["size"] = a["download_size"] = big.get("size") or 0
    a["sha256"] = big.get("sha256") or "-"
    a["verified"] = True          # a concrete downloadable asset was confirmed
    enriched += 1


# ---- production catalog: AppImages VERIFIED to run on this architecture -----
# work/i686-verified.json is produced by work/discover.py, which sweeps the whole
# AppImageHub corpus through GitHub's release endpoints and decides architecture
# by DECODING THE ELF HEADER of each asset - never from a filename.  That probe
# is also the dead-link check, so every entry below is known-downloadable.
# Metadata (description, publisher, licence, categories, icon, screenshots)
# comes from the app's own AppImageHub record.

# Editorial polish for apps we describe better than their upstream blurb.
VERIFIED_META = {
 "appimagetool": dict(name="AppImage Tool",
    summary="Official command-line tool that builds AppImages",
    developer="AppImage project", license="MIT", homepage="https://appimage.org"),
 "appimageupdate": dict(name="AppImageUpdate",
    summary="Update AppImages in place using efficient delta downloads",
    developer="AppImage project", license="MIT",
    homepage="https://github.com/AppImageCommunity/AppImageUpdate"),
}

ATTEMPTS = os.path.join(WORK, "icon-attempts.json")
try:    _attempted = set(json.load(open(ATTEMPTS)))
except Exception: _attempted = set()
_icon_budget = [float(os.environ.get("XXRI_ICON_BUDGET", "600"))]   # seconds
_icon_t0 = time.time()

def icon_for(aid, item, appimage_url, size):
    """Every app must have a real icon.  AppImageHub first; otherwise extract
    one out of the AppImage itself (.DirIcon / *.png in the payload root)."""
    dest = os.path.join(ICONS, aid + ".png")
    if os.path.exists(dest) and os.path.getsize(dest) > 0: return "hub"
    icons = (item or {}).get("icons") or []
    if icons:
        try:
            raw = fetch(HUBDB + icons[0], binary=True, timeout=25)
            tmp = dest + ".tmp"; open(tmp, "wb").write(raw)
            r = subprocess.run(["magick", tmp, "-resize", "128x128>", "-background", "none",
                                "-gravity", "center", "-extent", "128x128",
                                "-define", "png:color-type=6", "-depth", "8", dest],
                               capture_output=True)
            os.path.exists(tmp) and os.unlink(tmp)
            if r.returncode == 0 and os.path.exists(dest): return "hub"
        except Exception: pass
    # extract from the AppImage payload - once per app, inside a global budget
    if aid in _attempted: return "generated"
    if time.time() - _icon_t0 > _icon_budget[0]: return "generated"
    if appimage_url and 0 < size <= 130 * 1024 * 1024:
        _attempted.add(aid)
        try: json.dump(sorted(_attempted), open(ATTEMPTS, "w"))
        except Exception: pass
        try:
            tmpimg = os.path.join(WORK, "iconsrc.AppImage")
            with urllib.request.urlopen(urllib.request.Request(
                    appimage_url, headers={"User-Agent": "xxri-store-catalog"}), timeout=180) as r:
                with open(tmpimg, "wb") as f:
                    while True:
                        b = r.read(262144)
                        if not b: break
                        f.write(b)
            with open(tmpimg, "rb") as f: hdr = f.read(64)
            off = int.from_bytes(hdr[0x20:0x24], "little") + \
                  int.from_bytes(hdr[0x2e:0x30], "little") * int.from_bytes(hdr[0x30:0x32], "little")
            ex = os.path.join(WORK, "iconx"); subprocess.run(["rm", "-rf", ex], capture_output=True)
            subprocess.run(["unsquashfs", "-n", "-f", "-d", ex, "-o", str(off), tmpimg,
                            ".DirIcon", "*.png", "*.svg"], capture_output=True, timeout=120)
            cand = []
            for root, _d, files in os.walk(ex):
                for fn in files:
                    if fn == ".DirIcon" or fn.lower().endswith((".png", ".svg")):
                        cand.append(os.path.join(root, fn))
            cand.sort(key=lambda f: (0 if os.path.basename(f) == ".DirIcon" else 1,
                                     -os.path.getsize(f)))
            for c in cand:
                r = subprocess.run(["magick", c, "-resize", "128x128>", "-background", "none",
                                    "-gravity", "center", "-extent", "128x128",
                                    "-define", "png:color-type=6", "-depth", "8", dest],
                                   capture_output=True)
                if r.returncode == 0 and os.path.exists(dest) and os.path.getsize(dest) > 0:
                    subprocess.run(["rm", "-rf", ex, tmpimg], capture_output=True)
                    return "extracted"
            subprocess.run(["rm", "-rf", ex, tmpimg], capture_output=True)
        except Exception:
            pass
    return "generated"

VERIFIED_FILE = os.path.join(WORK, "i686-verified.json")
try:    BLACKLIST = json.load(open(os.path.join(WORK, "blacklist.json")))
except Exception: BLACKLIST = {}
verified_added = 0
blacklisted = 0
icon_src_count = {"hub": 0, "extracted": 0, "generated": 0}
if os.path.exists(VERIFIED_FILE):
    vdata = json.load(open(VERIFIED_FILE))
    # Ranking (Phase 9.5).  Mainstream software must lead the home page, so the
    # score combines: curated tier, GitHub stars, whether the app is actually
    # installable here, category importance, release recency and metadata
    # richness.  Nothing is ranked alphabetically.
    CAT_WEIGHT = {"browser": 60, "office": 60, "development": 55, "graphics": 50,
                  "video": 50, "audio": 45, "communication": 45, "security": 40,
                  "networking": 35, "productivity": 35, "system": 30, "utilities": 25,
                  "photography": 35, "games": 30, "education": 30, "science": 25,
                  "finance": 25, "virtualization": 30, "ai": 35, "accessories": 20}
    TIER_SCORE = {1: 1000, 2: 600, 3: 300, 4: 120}

    def _score(v):
        it = v.get("item") or {}
        s = 0
        # 1. curated importance - a flagship app outranks anything obscure
        s += TIER_SCORE.get(v.get("tier") or 0, 0)
        # 2. popularity: GitHub stars, compressed so 100k does not swamp tiers
        st = v.get("stars") or 0
        if st: s += min(int((st ** 0.5) * 4), 800)
        # 3. installable here beats "available for Regular/Pro"
        if v.get("arch") == "i686": s += 500
        if v.get("compat_ok"): s += 250
        # 4. category importance
        s += CAT_WEIGHT.get(v.get("seed_category") or "", 20)
        # 5. release recency
        d = (v.get("date") or v.get("tag_date") or "")[:4]
        if d.isdigit():
            yr = int(d)
            s += max(0, 200 - (2026 - yr) * 40)
        # 6. metadata richness (an entry that looks complete ranks above a stub)
        s += (len(it.get("screenshots") or []) * 5 + len(it.get("icons") or []) * 8
              + (10 if it.get("license") else 0)
              + min(len(it.get("description") or ""), 200) // 20)
        return s
    for repo, v in sorted(vdata.items(), key=lambda kv: -_score(kv[1])):
        it = v.get("item") or {}
        raw_name = v.get("seed_name") or v.get("feed_name") or repo.split(":")[-1]
        aid = slug(raw_name)
        if aid in BLACKLIST:          # failed the compatibility verifier
            blacklisted += 1; continue
        if v.get("arch") == "i686" and v.get("compat_ok") is False:
            blacklisted += 1; continue
        if aid in apps and apps[aid].get("verified"): continue
        desc = clean_html(it.get("description") or "")
        authors = it.get("authors") or []
        dev = (authors[0].get("name") if authors else "") or repo.split(":")[-1].split("/")[0]
        home = (authors[0].get("url") if authors else "") or ("https://github.com/" + repo.split(":")[-1])
        ov = VERIFIED_META.get(aid, {})
        name = ov.get("name") or pretty(raw_name)
        summ = ov.get("summary") or ellipsize(first_sentence(desc, 110), 110) or (name + " for XXRI OS Lite")
        headline = name + ". " + first_sentence(desc or name)
        cat = v.get("seed_category") or (pick_category(it, headline) if it else "utilities")
        kind_src = "github" if repo.startswith("github:") else ("gitlab" if repo.startswith("gitlab:") else "direct")
        ref = repo.split(":", 1)[1] if ":" in repo else ""
        shots = [HUBDB + s for s in (it.get("screenshots") or [])][:3]
        isrc = icon_for(aid, it, v.get("url"), v.get("size") or 0)
        icon_src_count[isrc] = icon_src_count.get(isrc, 0) + 1
        kw = [name.lower(), dev.lower(), cat] + [c.lower() for c in (it.get("categories") or []) if c]
        apps[aid] = dict(
            id=aid, name=name, category=cat, kind="appimage",
            summary=summ, description=desc or summ,
            developer=ov.get("developer") or dev, publisher=ov.get("developer") or dev,
            license=ov.get("license") or it.get("license") or "Open Source",
            homepage=ov.get("homepage") or home, website=ov.get("homepage") or home,
            arch=[v.get("arch") or "i686"], version=(v.get("tag") or "latest").lstrip("v"),
            release_date="", updated="",
            size=v.get("size", 0), download_size=v.get("size", 0), sha256="-",
            source_type=kind_src, source_ref=(ref + " *i686*.AppImage") if kind_src == "github" else "",
            url=v["url"],
            assets={(v.get("arch") or "i686"): {"url": v["url"], "size": v.get("size", 0), "sha256": "-"}},
            icon_url=(HUBDB + (it.get("icons") or [""])[0]) if it.get("icons") else "",
            screenshots=shots,
            permissions=["Network access", "Local files", "Desktop integration"],
            changelog="", dependencies=[], tags=[], keywords=sorted(set(kw)),
            rank=_score(v), verified=True,
            compat_ok=bool(v.get("compat_ok", True)),
            compat_reason=v.get("compat_reason", ""))
        verified_added += 1

# ---- mainstream apps that ship an official 32-bit PORTABLE build -----------
# Several applications everyone expects never published an i686 AppImage but do
# publish an official 32-bit tarball.  XXRI installs those through
# `xxri-app install-archive`, so from the user's side they behave identically.
# Each URL is probed at build time and dropped if it does not resolve.
PORTABLE = [
 dict(id="telegram", name="Telegram Desktop", category="communication",
      url="https://telegram.org/dl/desktop/linux32", version="linux32",
      exec="Telegram/Telegram", icon="",
      summary="Fast, secure messaging with cloud sync",
      description=("Telegram Desktop is a fast and secure messenger that syncs across all your "
                   "devices. Telegram publishes an official 32-bit Linux build, which is the one "
                   "XXRI OS Lite installs."),
      developer="Telegram FZ-LLC", license="GPL-3.0", homepage="https://telegram.org"),
 dict(id="code-oss", name="Visual Studio Code", category="development",
      url="https://update.code.visualstudio.com/1.35.1/linux-ia32/stable", version="1.35.1",
      exec="VSCode-linux-ia32/code", icon="VSCode-linux-ia32/resources/app/resources/linux/code.png",
      summary="The popular code editor from Microsoft",
      description=("Visual Studio Code is a lightweight but powerful source code editor with "
                   "built-in Git, debugging and extensions. Version 1.35.1 is the last release "
                   "Microsoft published for 32-bit Linux, and it is the build XXRI installs."),
      developer="Microsoft", license="MIT", homepage="https://code.visualstudio.com"),
 dict(id="firefox", name="Mozilla Firefox", category="browser",
      url="https://ftp.mozilla.org/pub/firefox/releases/115.14.0esr/linux-i686/en-US/firefox-115.14.0esr.tar.bz2",
      version="115.14.0esr", exec="firefox/firefox", icon="firefox/browser/chrome/icons/default/default128.png",
      summary="The independent, privacy-first web browser",
      description=("Mozilla Firefox is a fast, private and open-source web browser. The 115 ESR "
                   "series is the final line Mozilla builds for 32-bit Linux and receives "
                   "security updates, which is why XXRI OS Lite ships that release."),
      developer="Mozilla", license="MPL-2.0", homepage="https://www.mozilla.org/firefox/"),
 dict(id="thunderbird", name="Mozilla Thunderbird", category="browser",
      url="https://ftp.mozilla.org/pub/thunderbird/releases/115.14.0/linux-i686/en-US/thunderbird-115.14.0.tar.bz2",
      version="115.14.0", exec="thunderbird/thunderbird", icon="thunderbird/chrome/icons/default/default128.png",
      summary="Full-featured email, calendar and contacts",
      description=("Thunderbird is a free email client with a unified inbox, calendar, address "
                   "book and message filters. Version 115 is the last 32-bit Linux line."),
      developer="MZLA Technologies", license="MPL-2.0", homepage="https://www.thunderbird.net"),
 dict(id="torbrowser", name="Tor Browser", category="browser",
      url="https://archive.torproject.org/tor-package-archive/torbrowser/10.5.10/tor-browser-linux32-10.5.10_en-US.tar.xz",
      version="10.5.10", exec="tor-browser_en-US/Browser/start-tor-browser", icon="",
      summary="Browse anonymously through the Tor network",
      description=("Tor Browser routes your traffic through the Tor network to resist tracking "
                   "and surveillance. 10.5.10 is the last release built for 32-bit Linux."),
      developer="The Tor Project", license="MPL-2.0", homepage="https://www.torproject.org"),
 dict(id="blender", name="Blender", category="graphics",
      url="https://download.blender.org/release/Blender2.79/blender-2.79-linux-glibc219-i686.tar.bz2",
      version="2.79", exec="blender-2.79-linux-glibc219-i686/blender", icon="",
      summary="Professional 3D modelling, animation and rendering",
      description=("Blender is a complete 3D creation suite covering modelling, sculpting, "
                   "animation, simulation and rendering. Release 2.79 is the final version the "
                   "Blender Foundation published for 32-bit Linux."),
      developer="Blender Foundation", license="GPL-2.0", homepage="https://www.blender.org"),
 dict(id="doublecmd", name="Double Commander", category="utilities",
      url="https://github.com/doublecmd/doublecmd/releases/download/v1.2.7/doublecmd-1.2.7.gtk2.i386.tar.xz",
      version="1.2.7", exec="doublecmd/doublecmd", icon="doublecmd/pixmaps/dcicon.png",
      summary="Two-panel file manager in the Total Commander tradition",
      description=("Double Commander is a cross-platform orthodox file manager with two panels "
                   "side by side, built-in viewer, batch rename and archive handling. The "
                   "project still publishes current i386 Linux builds."),
      developer="Alexander Koblov", license="GPL-2.0", homepage="https://doublecmd.sourceforge.io"),
 dict(id="ffmpeg", name="FFmpeg", category="video",
      url="https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-i686-static.tar.xz",
      version="release", exec="", icon="",
      summary="The complete audio and video conversion toolkit",
      description=("FFmpeg converts, records and streams practically every audio and video "
                   "format in existence. This is the officially recommended static i686 build, "
                   "so it runs without pulling in any codec libraries."),
      developer="FFmpeg team", license="GPL-3.0", homepage="https://ffmpeg.org"),
]

# ---- mainstream apps that ship a NATIVE i686 build as a Tiny Core extension --
# Upstream stopped publishing 32-bit AppImages for most of the desktop staples,
# but the Tiny Core i686 repository still builds them from source. These are
# real, current, native 32-bit binaries - the best possible fit for this device.
NATIVE = [
 dict(id="gimp", tcz="gimp", name="GIMP", category="graphics", exec="gimp",
      summary="Professional image editing and photo retouching",
      description=("GIMP is a full-featured raster graphics editor for photo retouching, image "
                   "composition and authoring, with layers, masks, filters and a scripting "
                   "engine. This is a native 32-bit build."),
      developer="The GIMP Team", license="GPL-3.0", homepage="https://www.gimp.org"),
 dict(id="inkscape", tcz="inkscape", name="Inkscape", category="graphics", exec="inkscape",
      summary="Professional vector graphics editor",
      description=("Inkscape is a vector drawing program using SVG as its native format, with "
                   "bezier editing, boolean operations, gradients, clones and live path "
                   "effects. This is a native 32-bit build."),
      developer="Inkscape Project", license="GPL-3.0", homepage="https://inkscape.org"),
 dict(id="vlc", tcz="vlc", name="VLC media player", category="video", exec="vlc",
      summary="Plays every audio and video file you have",
      description=("VLC is the media player that plays practically every format without needing "
                   "extra codecs, including DVDs, network streams and damaged files. This is a "
                   "native 32-bit build."),
      developer="VideoLAN", license="GPL-2.0", homepage="https://www.videolan.org"),
 dict(id="audacity", tcz="audacity", name="Audacity", category="audio", exec="audacity",
      summary="Multi-track audio recorder and editor",
      description=("Audacity records and edits audio across unlimited tracks, with effects, "
                   "noise reduction, spectral analysis and export to every common format. "
                   "This is a native 32-bit build."),
      developer="Audacity Team", license="GPL-2.0", homepage="https://www.audacityteam.org"),
 dict(id="geany", tcz="geany", name="Geany", category="development", exec="geany",
      summary="Fast, light IDE for dozens of languages",
      description=("Geany is a small and fast development environment with syntax highlighting, "
                   "code folding, symbol lists and build commands for over 50 languages, while "
                   "starting almost instantly."),
      developer="The Geany contributors", license="GPL-2.0", homepage="https://www.geany.org"),
 dict(id="bluefish", tcz="bluefish", name="Bluefish", category="development", exec="bluefish",
      summary="Powerful editor for web developers and programmers",
      description=("Bluefish is an editor aimed at web developers, with project support, "
                   "multi-file search and replace, snippets and toolbars for HTML, PHP, "
                   "Python and more."),
      developer="Bluefish Team", license="GPL-3.0", homepage="https://bluefish.openoffice.nl"),
 dict(id="abiword", tcz="abiword", name="AbiWord", category="office", exec="abiword",
      summary="Fast, full-featured word processor",
      description=("AbiWord is a word processor that reads and writes .doc, .docx, .odt and RTF, "
                   "with styles, tables, mail merge and footnotes, while staying small enough "
                   "to open instantly."),
      developer="AbiSource", license="GPL-2.0", homepage="https://www.abisource.com"),
 dict(id="gnumeric", tcz="gnumeric", name="Gnumeric", category="office", exec="gnumeric",
      summary="Accurate, fast spreadsheet application",
      description=("Gnumeric is a spreadsheet with a very large function library, strong "
                   "statistical analysis and excellent Excel file compatibility, in a fraction "
                   "of the size of a full office suite."),
      developer="The GNOME Project", license="GPL-2.0", homepage="http://www.gnumeric.org"),
 dict(id="mtpaint", tcz="mtpaint", name="mtPaint", category="graphics", exec="mtpaint",
      summary="Lightweight pixel and icon editor",
      description=("mtPaint is a painting program for creating icons, pixel art and simple "
                   "image edits. It is tiny, starts instantly and handles indexed palettes "
                   "precisely."),
      developer="Mark Tyler", license="GPL-3.0", homepage="https://mtpaint.sourceforge.net"),
 dict(id="gparted", tcz="gparted", name="GParted", category="utilities", exec="gparted",
      summary="Graphical partition editor for disks",
      description=("GParted creates, resizes, moves, copies and checks disk partitions and their "
                   "file systems, with a clear graphical map of every drive."),
      developer="GParted Project", license="GPL-2.0", homepage="https://gparted.org"),
 dict(id="remmina", tcz="remmina", name="Remmina", category="networking", exec="remmina",
      summary="Remote desktop client for RDP, VNC and SSH",
      description=("Remmina connects to remote computers over RDP, VNC, SPICE and SSH from a "
                   "single window, with saved profiles and a tabbed interface."),
      developer="Remmina Project", license="GPL-2.0", homepage="https://remmina.org"),
 dict(id="p7zip", tcz="p7zip", name="7-Zip", category="utilities", exec="7z",
      summary="High-ratio archiver for 7z, ZIP, TAR and more",
      description=("7-Zip packs and unpacks 7z, ZIP, GZIP, BZIP2, XZ and TAR archives and reads "
                   "RAR, CAB, ISO and many others, with strong AES-256 encryption."),
      developer="Igor Pavlov", license="LGPL-2.1", homepage="https://www.7-zip.org"),
 dict(id="qemu", tcz="qemu", name="QEMU", category="virtualization", exec="qemu-system-i386",
      summary="Run other operating systems in a virtual machine",
      description=("QEMU is a machine emulator and virtualiser that runs whole operating systems "
                   "in a window, useful for testing, legacy software and development."),
      developer="QEMU Project", license="GPL-2.0", homepage="https://www.qemu.org"),
]

def head_ok(url):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "xxri-store-catalog",
                                                   "Range": "bytes=0-1023"})
        with urllib.request.urlopen(req, timeout=45) as r:
            cr = r.headers.get("Content-Range") or ""
            r.read(16)
            return True, (int(cr.split("/")[-1]) if "/" in cr else 0)
    except Exception:
        return False, 0

portable_added = 0
for pa in PORTABLE:
    ok, sz = head_ok(pa["url"])
    if not ok:
        print("   - dropped %s (portable URL does not resolve)" % pa["id"]); continue
    aid = pa["id"]
    apps[aid] = dict(
        id=aid, name=pa["name"], category=pa["category"], kind="archive",
        summary=pa["summary"], description=pa["description"],
        developer=pa["developer"], publisher=pa["developer"], license=pa["license"],
        homepage=pa["homepage"], website=pa["homepage"],
        arch=["i686"], version=pa["version"], release_date="", updated="",
        size=sz, download_size=sz, sha256="-",
        source_type="direct", source_ref="", url=pa["url"],
        assets={"i686": {"url": pa["url"], "size": sz, "sha256": "-"}},
        icon_url="", screenshots=[],
        permissions=["Network access", "Local files", "Desktop integration"],
        changelog="", dependencies=[], tags=[], keywords=[pa["name"].lower(), pa["category"]],
        rank=3000 - portable_added, verified=True, compat_ok=True,
        exec_path=pa["exec"], icon_path=pa["icon"])
    portable_added += 1
    print("   + portable %-12s %-10s %.1f MB" % (aid, pa["version"], sz / 1048576 if sz else 0))

# Applications the user explicitly asked for that have NO 32-bit Linux build.
# They are listed so the catalog reflects reality rather than pretending they do
# not exist, but they can never show an Install button: the Store renders the
# reason below instead. Every claim here was verified against the live servers.
UNAVAILABLE = [
 dict(id="onlyoffice", name="ONLYOFFICE Desktop Editors", category="office",
      developer="Ascensio System SIA", license="AGPL-3.0",
      homepage="https://www.onlyoffice.com/desktop.aspx",
      summary="Office suite compatible with DOCX, XLSX and PPTX",
      description=("ONLYOFFICE Desktop Editors is a full office suite with word processing, "
                   "spreadsheets and presentations, built around the OOXML formats.\n\n"
                   "No 32-bit Linux build exists. On download.onlyoffice.com the Linux "
                   "packages are published only as onlyoffice-desktopeditors_amd64.deb, "
                   "onlyoffice-desktopeditors.x86_64.rpm and DesktopEditors-x86_64.AppImage "
                   "(all HTTP 200), while every i386/i686 equivalent returns HTTP 404. The "
                   "36 assets named \u201cx86\u201d across the last 10 GitHub releases are "
                   "Windows builds (.exe, .msi, _xp.exe and a .zip whose first member is "
                   "app.ico and whose tree contains .dll files - no ELF anywhere)."),
      reason=("ONLYOFFICE publishes no 32-bit Linux build. Its Linux packages are x86_64 only, "
              "and the assets named \u201cx86\u201d are Windows installers.")),
]
for ua in UNAVAILABLE:
    aid = ua["id"]
    apps[aid] = dict(
        id=aid, name=ua["name"], category=ua["category"], kind="unavailable",
        summary=ua["summary"], description=ua["description"],
        developer=ua["developer"], publisher=ua["developer"], license=ua["license"],
        homepage=ua["homepage"], website=ua["homepage"],
        arch=["x86_64"], version="-", release_date="", updated="",
        size=0, download_size=0, sha256="-",
        source_type="none", source_ref="", url="",
        assets={}, icon_url="", screenshots=[],
        permissions=[], changelog="", dependencies=[], tags=[],
        keywords=[ua["name"].lower(), ua["category"], "office suite", "docx"],
        rank=2500, verified=False, compat_ok=False,
        exec_path="", icon_path="", unavailable=ua["reason"])
    print("   ! unavailable %-12s (%s)" % (aid, "no 32-bit Linux build"))

# Native extensions: confirm the .tcz is really published before listing it,
# and record the on-wire size so the download manager can show a real bar.
TCZ_BASE = os.environ.get("TCZ_BASE", "http://repo.tinycorelinux.net/16.x/x86/tcz")

# How well known the app is, independent of how it happens to be packaged - a
# native extension of GIMP and a portable Firefox both belong at the very top,
# so the home page cannot be led by whichever list was appended last.
FAME = {"firefox": 60, "gimp": 59, "vlc": 58, "telegram": 57, "inkscape": 56,
        "code-oss": 55, "blender": 54, "audacity": 53, "thunderbird": 52,
        "torbrowser": 51, "gparted": 50, "qemu": 49, "geany": 48,
        "abiword": 47, "gnumeric": 46, "p7zip": 45, "doublecmd": 44,
        "ffmpeg": 43, "remmina": 42, "bluefish": 41, "mtpaint": 40}
native_added = 0
for na in NATIVE:
    url = "%s/%s.tcz" % (TCZ_BASE, na["tcz"])
    ok, sz = head_ok(url)
    if not ok:
        print("   - dropped %s (extension not published)" % na["id"]); continue
    aid = na["id"]
    apps[aid] = dict(
        id=aid, name=na["name"], category=na["category"], kind="tcz",
        summary=na["summary"], description=na["description"],
        developer=na["developer"], publisher=na["developer"], license=na["license"],
        homepage=na["homepage"], website=na["homepage"],
        arch=["i686"], version="native", release_date="", updated="",
        size=sz, download_size=sz, sha256="-",
        source_type="tcz", source_ref=na["tcz"], url=url,
        assets={"i686": {"url": url, "size": sz, "sha256": "-"}},
        icon_url="", screenshots=[],
        permissions=["Local files", "Desktop integration"],
        changelog="", dependencies=[], tags=[],
        keywords=[na["name"].lower(), na["category"], "native", "32-bit"],
        rank=4000 - native_added, verified=True, compat_ok=True,
        exec_path=na["exec"], icon_path="")
    native_added += 1
    print("   + native   %-12s %-10s %.1f MB" % (aid, "i686", sz / 1048576 if sz else 0))

for _aid, _f in FAME.items():
    if _aid in apps:
        apps[_aid]["rank"] = 3000 + _f


# Home-page sections must describe what this device can actually install, so
# the editorial tags are rebuilt from scratch over the verified i686 set - any
# tag inherited from the wider (x86_64) catalog would fill the rails with apps
# the Store then hides, leaving them empty.
for _a in apps.values(): _a["tags"] = []
# Featured/Recommended come ONLY from apps this device can install, ranked by
# popularity (tier + stars + recency + category weight).  Anything the Store
# would hide can never reach a home-page rail and leave it looking empty.
_ranked = [a for a in apps.values()
           if a.get("verified") and "i686" in a["arch"] and a.get("compat_ok", True)]
_ranked.sort(key=lambda a: -a["rank"])
_i686 = _ranked[:40]
for n, a in enumerate(_i686):
    t = []
    if n < 8: t.append("featured")
    if n < 18: t.append("recommended")
    if 4 <= n < 16: t.append("trending")
    if n % 3 == 0 and n < 24: t.append("editors")
    if 8 <= n < 20: t.append("newest")
    a["tags"] = t

# ------------------------------------------------------------- bundled icons --
# Real icons for the curated top apps (the offline home page looks like the
# mockup); every other app gets a gradient tile drawn by the Store at runtime.
def bundle_icon(a):
    dest = os.path.join(ICONS, a["id"] + ".png")
    if os.path.exists(dest) and os.path.getsize(dest) > 0: return True
    if not a.get("icon_url"): return False
    try:
        raw = fetch(a["icon_url"], binary=True, timeout=20)
        tmp = dest + ".tmp"
        open(tmp, "wb").write(raw)
        # normalise to a 128x128 8-bit RGBA PNG (the GUI reads PNGs via cairo)
        r = subprocess.run(["magick", tmp, "-resize", "128x128>", "-background", "none",
                            "-gravity", "center", "-extent", "128x128",
                            "-define", "png:color-type=6", "-depth", "8", dest],
                           capture_output=True)
        os.path.exists(tmp) and os.unlink(tmp)
        return r.returncode == 0 and os.path.exists(dest)
    except Exception:
        return False

# NOTE: icons are never wiped wholesale here - the verified-app block above has
# already fetched/extracted real icons for the installable apps.  Stale icons are
# pruned at the very end, by catalog membership.
top = sorted(apps.values(), key=lambda a: -a["rank"])[:70]
got_icons = sum(1 for a in top if bundle_icon(a))

# ----------------------------------------------------------------- emit ------
bycat = {}
for a in apps.values(): bycat.setdefault(a["category"], []).append(a)
for cat in bycat: bycat[cat].sort(key=lambda a: (-a["rank"], a["name"].lower()))

titles = dict(CATS)
for cat in CAT_IDS:
    lst = bycat.get(cat, [])
    with open(os.path.join(REPO, cat + ".json"), "w") as f:
        json.dump({"format": FMT_VERSION, "category": cat, "title": titles[cat],
                   "count": len(lst), "apps": lst}, f, separators=(",", ":"), ensure_ascii=False)

# index.tsv - the flat index the busybox engine reads.  Columns are positional
# and APPEND-ONLY: new columns never break an older backend.
#  1 id  2 name  3 category  4 archlist  5 version  6 size  7 sha256
#  8 source_type  9 source_ref  10 url  11 kind  12 updated  13 assets
#  14 icon_url  15 screenshots(;)  16 verified(1/0)  17 exec  18 icon-in-archive
def assets_col(a):
    return ";".join("%s,%s,%s,%s" % (ar, v.get("url",""), v.get("sha256","-") or "-", v.get("size",0))
                    for ar, v in sorted(a.get("assets", {}).items()) if v.get("url"))
rows = []
for cat in CAT_IDS:
    for a in bycat.get(cat, []):
        rows.append("\t".join(str(x).replace("\t"," ") for x in [
            a["id"], a["name"], a["category"], " ".join(a["arch"]), a["version"],
            a["size"], a["sha256"], a["source_type"], a["source_ref"], a["url"],
            a["kind"], a["updated"], assets_col(a), a["icon_url"], ";".join(a["screenshots"]),
            1 if a.get("verified") else 0, a.get("exec_path",""), a.get("icon_path","")]))
with open(os.path.join(REPO, "index.tsv"), "w") as f:
    f.write("\n".join(rows) + "\n")

def ids_with(tag, n=None):
    v = [a["id"] for a in sorted(apps.values(), key=lambda a: -a["rank"]) if tag in a["tags"]]
    return v[:n] if n else v
recent = [a["id"] for a in sorted([a for a in apps.values() if a["updated"]],
                                  key=lambda a: a["updated"], reverse=True)][:12]
sections = {
 "format": FMT_VERSION,
 "featured": ids_with("featured", 8),
 "recommended": ids_with("recommended", 18),
 "trending": ids_with("trending", 12),
 "editors": ids_with("editors", 10),
 "newest": ids_with("newest", 8) or recent[:8],
 "updated": recent,
 "categories": CAT_IDS,
}
with open(os.path.join(REPO, "sections.json"), "w") as f: json.dump(sections, f, separators=(",", ":"), ensure_ascii=False)

meta = {"format": FMT_VERSION, "generated": time.strftime("%Y-%m-%d"),
        "apps": len(apps), "categories": len(CAT_IDS),
        "kinds": ["appimage"], "source": "AppImageHub + GitHub Releases + XXRI curated",
        "note": "append-only index columns; unknown kinds are ignored by older clients"}
with open(os.path.join(REPO, "meta.json"), "w") as f: json.dump(meta, f, separators=(",", ":"), ensure_ascii=False)

# prune icons that no longer belong to any catalogued app
_pruned = 0
for old in os.listdir(ICONS):
    if not old.endswith(".png"): continue
    if old[:-4] not in apps:
        os.unlink(os.path.join(ICONS, old)); _pruned += 1

print("catalog: %d apps / %d categories (skipped %d feed items)" % (len(apps), len(CAT_IDS), skipped))
print("icons: %d hub, %d extracted from AppImage, %d placeholder, %d pruned" % (
      icon_src_count.get("hub",0), icon_src_count.get("extracted",0),
      icon_src_count.get("generated",0), _pruned))
print("curated extras: %d verified, %d dropped" % (extras_ok, extras_dropped))
print("portable (archive) apps added: %d" % portable_added)
print("ELF-verified apps added: %d  (blacklisted by compatibility verifier: %d)"
      % (verified_added, blacklisted))
print("enriched from GitHub API: %d   bundled icons: %d/%d" % (enriched, got_icons, len(top)))
for cat in CAT_IDS:
    print("   %-16s %4d" % (cat, len(bycat.get(cat, []))))
inst = [a["id"] for a in apps.values() if "i686" in a["arch"]]
print("installable on i686: %s" % (", ".join(inst) or "none"))
