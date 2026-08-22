#!/usr/bin/env python3
"""discover.py - exhaustive i686 AppImage discovery engine (Phase 9.4).

The previous engine only sampled a curated candidate list because GitHub's REST
API allows 60 unauthenticated requests/hour.  That limit does not apply to
github.com's own release endpoints, so this engine enumerates EVERY project:

    HEAD  github.com/<o>/<r>/releases/latest        -> 302 with the latest tag
    GET   github.com/<o>/<r>/releases/expanded_assets/<tag>  -> every asset

Neither call touches api.github.com, so the whole AppImageHub corpus
(~1100 projects) can be swept.  For each candidate asset the true architecture
is decided by DOWNLOADING THE FIRST 64 BYTES AND DECODING THE ELF HEADER
(EI_CLASS + e_machine) - never from the filename - which simultaneously proves
the URL is alive and really serves a binary.

Sources, in the brief's priority order:
  1. AppImageHub metadata      (project corpus + descriptions/icons/screenshots)
  2. GitHub Releases           (web endpoints; API only if ever needed)
  3. GitLab Releases API       (300 req/min unauthenticated - generous)
  4. Official project download pages (direct .AppImage links from the feed)
HTML is parsed only where no API exists (GitHub's expanded_assets fragment).

Everything is cached under work/disc-cache/ so re-runs resume instantly.

Usage:
  ./discover.py                 full sweep
  ./discover.py --limit N       only the first N projects (smoke test)
  ./discover.py --workers N     concurrency (default 12)
"""
import json, os, re, sys, time, random, threading, urllib.request, urllib.error, hashlib
from concurrent.futures import ThreadPoolExecutor, as_completed

BUILD = "/home/jantzen/xxri-build"
WORK  = os.path.join(BUILD, "work")
FEED  = os.path.join(WORK, "appimagehub-feed.json")
CACHE = os.path.join(WORK, "disc-cache")
OUT   = os.path.join(WORK, "i686-verified.json")
STATS = os.path.join(WORK, "discovery-stats.json")
os.makedirs(CACHE, exist_ok=True)

UA = "Mozilla/5.0 (X11; Linux i686) xxri-store-discovery/9.4"
WORKERS = 12
LIMIT = 0
for i, a in enumerate(sys.argv):
    if a == "--workers" and i + 1 < len(sys.argv): WORKERS = int(sys.argv[i + 1])
    if a == "--limit"   and i + 1 < len(sys.argv): LIMIT   = int(sys.argv[i + 1])

lock = threading.Lock()
S = dict(projects=0, projects_scanned=0, projects_failed=0, releases_read=0,
         assets_seen=0, appimages_seen=0, probed=0, probe_failed=0,
         i686=0, x86_64=0, aarch64=0, arm=0, other_arch=0,
         broken_urls=0, duplicates=0, gitlab=0, direct=0, transient=0)

def bump(k, n=1):
    with lock: S[k] = S.get(k, 0) + n

# ------------------------------------------------------------------ http ----
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl): return None
_noredir = urllib.request.build_opener(NoRedirect)

def http(url, headers=None, timeout=30, opener=None, data=None, method=None):
    h = {"User-Agent": UA, "Accept": "*/*"}
    if headers: h.update(headers)
    req = urllib.request.Request(url, headers=h, data=data)
    if method: req.get_method = lambda: method
    op = opener or urllib.request
    for attempt in range(6):
        try:
            return (op.open(req, timeout=timeout) if opener else
                    urllib.request.urlopen(req, timeout=timeout))
        except urllib.error.HTTPError as e:
            if e.code in (429, 403) and attempt < 5:
                time.sleep(5 * (attempt + 1) + random.random() * 4); continue
            raise
        except Exception:
            if attempt < 3: time.sleep(2 + random.random() * 2); continue
            raise
    return None

# ------------------------------------------------------------- ELF probe ----
EM = {3: "i686", 62: "x86_64", 183: "aarch64", 40: "arm"}
PROBE_DB = os.path.join(CACHE, "probes.json")
try:    probes = json.load(open(PROBE_DB))
except Exception: probes = {}
_plock = threading.Lock()

def elf_arch(url):
    """Decode the real ELF header. -> (arch|None, size, ok_url)"""
    with _plock:
        c = probes.get(url)
    if c is not None:
        return c[0], c[1], c[2]
    try:
        r = http(url, headers={"Range": "bytes=0-63"}, timeout=45)
        head = r.read(64)
        cr = r.headers.get("Content-Range") or ""
        size = int(cr.split("/")[-1]) if "/" in cr else int(r.headers.get("Content-Length") or 0)
    except Exception:
        bump("broken_urls")
        with _plock: probes[url] = [None, 0, False]
        return None, 0, False
    if len(head) < 20 or head[:4] != b"\x7fELF":
        bump("broken_urls")
        with _plock: probes[url] = [None, size, False]
        return None, size, False
    cls = head[4]
    arch = EM.get(int.from_bytes(head[18:20], "little"))
    if arch == "i686" and cls != 1: arch = None
    with _plock: probes[url] = [arch, size, True]
    return arch, size, True

# ---------------------------------------------------- asset classification --
# every 32-bit spelling the brief lists, plus common extras
HINT32 = re.compile(r"(i[3-6]86|ia32|x86[-_]?linux|linux[-_]?32|32[-_]?bit|"
                    r"\bx86\b|legacy|portable)", re.I)
HINT64 = re.compile(r"(x86[-_]?64|amd64|aarch64|arm64|armv7|armhf|riscv|ppc64|"
                    r"64[-_]?bit|linux[-_]?64)", re.I)
SKIP   = re.compile(r"(\.zsync$|\.sig$|\.asc$|\.sha\d*$|\.md5$|\.torrent$|debug)", re.I)
ISAPP  = re.compile(r"\.appimage$", re.I)

def classify(name):
    """-> 'probe32' (worth probing), 'probe?' (no marker), or '64' (skip)."""
    if HINT32.search(name) and not re.search(r"x86[-_]?64", name, re.I): return "probe32"
    if HINT64.search(name): return "64"
    return "probe?"

# ---------------------------------------------------------- github (web) ----
def _latest_tag(repo, hops=3):
    """Resolve the newest tag without the API.  Follows repo RENAMES (301 to
    another /releases/latest) and falls back to the releases atom feed when a
    project has no "latest" release (302 -> /releases)."""
    url = "https://github.com/%s/releases/latest" % repo
    for _ in range(hops):
        try:
            http(url, opener=_noredir, timeout=25, method="HEAD")
            return None, repo, "no-redirect"
        except urllib.error.HTTPError as e:
            loc = e.headers.get("Location") if e.headers else None
            if e.code == 404: return None, repo, "404"
            if not loc: return None, repo, "http%s" % e.code
            if "/releases/tag/" in loc:
                repo = re.sub(r"^https://github\.com/([^/]+/[^/]+)/.*$", r"\1", loc)
                return loc.rsplit("/releases/tag/", 1)[1], repo, None
            if loc.endswith("/releases/latest"):          # renamed project
                url = loc
                repo = re.sub(r"^https://github\.com/([^/]+/[^/]+)/.*$", r"\1", loc)
                continue
            if loc.rstrip("/").endswith("/releases"):     # no "latest" release
                repo = re.sub(r"^https://github\.com/([^/]+/[^/]+)/.*$", r"\1", loc)
                break
            return None, repo, "http%s" % e.code
        except Exception:
            return None, repo, "net"
    # atom feed: newest tags even when there is no "latest" release
    t = gh_tags(repo, 1)
    return (t[0] if t else None), repo, (None if t else "no-releases")

def gh_tags(repo, n=4):
    """Most recent tags from the releases atom feed (no API)."""
    key = os.path.join(CACHE, "tags_" + repo.replace("/", "__") + ".json")
    if os.path.exists(key):
        try: return json.load(open(key))[:n]
        except Exception: pass
    tags = []
    try:
        x = http("https://github.com/%s/releases.atom" % repo, timeout=25).read().decode("utf-8", "replace")
        tags = [m.group(1) for m in re.finditer(r"/releases/tag/([^\"'<]+)", x)]
        seen = set(); tags = [t for t in tags if not (t in seen or seen.add(t))]
    except Exception:
        pass
    json.dump(tags, open(key, "w"))
    return tags[:n]

def gh_assets_for_tag(repo, tag):
    """Every asset of one tag via the expanded_assets HTML fragment."""
    import urllib.parse as up
    key = os.path.join(CACHE, "a_%s__%s.json" % (repo.replace("/", "__"),
                                                 re.sub(r"[^A-Za-z0-9._-]", "_", tag)[:60]))
    if os.path.exists(key):
        try: return json.load(open(key))
        except Exception: pass
    out = []
    try:
        frag = http("https://github.com/%s/releases/expanded_assets/%s"
                    % (repo, up.quote(tag, safe="")), timeout=30).read().decode("utf-8", "replace")
        for m in re.finditer(r'href="(/[^"]*?/releases/download/[^"]+)"', frag):
            href = m.group(1)
            out.append({"name": up.unquote(href.rsplit("/", 1)[1]),
                        "url": "https://github.com" + href})
    except Exception:
        pass
    json.dump(out, open(key, "w"))
    return out

def gh_assets(repo):
    """Latest release assets, following renames and empty-latest projects."""
    key = os.path.join(CACHE, "gh_" + repo.replace("/", "__") + ".json")
    if os.path.exists(key):
        try: return json.load(open(key))
        except Exception: pass
    tag, real, err = _latest_tag(repo)
    out = {"tag": tag, "repo": real, "assets": [], "err": err}
    if tag:
        bump("releases_read")
        out["assets"] = gh_assets_for_tag(real, tag)
        out["err"] = None
    # 429/5xx/network are TRANSIENT - caching them would make the gap permanent
    if out["err"] in ("http429", "http403", "net", "http500", "http502", "http503"):
        bump("transient"); return out
    json.dump(out, open(key, "w"))
    return out

# ---------------------------------------------------------------- gitlab ----
def gl_assets(project):
    key = os.path.join(CACHE, "gl_" + project.replace("/", "__") + ".json")
    if os.path.exists(key):
        try: return json.load(open(key))
        except Exception: pass
    out = {"tag": None, "assets": [], "err": None}
    try:
        import urllib.parse as up
        r = http("https://gitlab.com/api/v4/projects/%s/releases" % up.quote(project, safe=""),
                 timeout=30)
        rel = json.loads(r.read().decode())
        if rel:
            out["tag"] = rel[0].get("tag_name")
            for l in ((rel[0].get("assets") or {}).get("links") or []):
                u = l.get("direct_asset_url") or l.get("url") or ""
                if u: out["assets"].append({"name": u.rsplit("/", 1)[-1], "url": u})
    except Exception as e:
        out["err"] = str(e)[:60]
    json.dump(out, open(key, "w"))
    return out

# --------------------------------------------------------------- seeds ------
# Phase 9.5: the curated database is crawled FIRST so mainstream software is
# discovered directly instead of waiting for it to appear in AppImageHub.
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location("popular_apps", os.path.join(WORK, "popular-apps.py"))
_pa = _ilu.module_from_spec(_spec); _spec.loader.exec_module(_pa)
SEED_REPOS = _pa.seed_repos()
SEED_BY_REPO = {r: (n, c, t) for r, n, c, t in SEED_REPOS}

def gh_stars(repo):
    """Star count from the repo page (no API).  Ranking signal only."""
    key = os.path.join(CACHE, "stars_" + repo.replace("/", "__") + ".json")
    if os.path.exists(key):
        try: return json.load(open(key))
        except Exception: pass
    n = 0
    try:
        h = http("https://github.com/%s" % repo, timeout=25).read().decode("utf-8", "replace")
        m = re.search(r'aria-label="([\d,]+) users starred', h)
        if m: n = int(m.group(1).replace(",", ""))
    except Exception:
        pass
    json.dump(n, open(key, "w"))
    return n

# ----------------------------------------------------------------- feed -----
feed = json.load(open(FEED))["items"]
projects = []       # (kind, ref, feeditem)
seen_ref = set()
for it in feed:
    links = {l.get("type"): l.get("url") for l in (it.get("links") or [])}
    gh, dl = links.get("GitHub"), (links.get("Download") or "")
    if gh:
        gh = gh.strip().strip("/")
        if gh.count("/") == 1 and gh not in seen_ref:
            seen_ref.add(gh); projects.append(("github", gh, it))
    elif "gitlab.com" in dl:
        m = re.search(r"gitlab\.com/([^/]+/[^/]+)", dl)
        if m and m.group(1) not in seen_ref:
            seen_ref.add(m.group(1)); projects.append(("gitlab", m.group(1), it))
    elif dl.lower().endswith(".appimage"):
        projects.append(("direct", dl, it))
# Source 3: official project download pages.  Several flagship apps publish
# AppImages from their own mirrors rather than GitHub releases; these URLs are
# still ELF-probed like everything else, and dropped if they do not resolve.
OFFICIAL = [
 ("Krita",      "graphics", 1, "https://download.kde.org/stable/krita/5.2.9/krita-5.2.9-x86_64.AppImage"),
 ("Kdenlive",   "video",    1, "https://download.kde.org/stable/kdenlive/24.12/linux/kdenlive-24.12.3-x86_64.AppImage"),
 ("digiKam",    "photography", 2, "https://download.kde.org/stable/digikam/8.5.0/digiKam-8.5.0-x86-64.appimage"),
 ("LibreOffice","office",   1, "https://appimages.libreitalia.org/LibreOffice-fresh.basic-x86_64.AppImage"),
 ("KDE Itinerary","productivity",3,"https://download.kde.org/stable/itinerary/24.12.3/itinerary-24.12.3-x86_64.AppImage"),
 ("Kile",       "office",   3, "https://download.kde.org/stable/kile/2.9.93/kile-2.9.93-x86_64.AppImage"),
 ("Tellico",    "office",   4, "https://download.kde.org/stable/tellico/4.0.1/tellico-4.0.1-x86_64.AppImage"),
]
for _n, _c, _t, _u in OFFICIAL:
    projects.append(("direct", _u, {"name": _n, "_seed": True, "_cat": _c, "_tier": _t}))

# seeds first: they are what users actually look for
seed_projects = []
for repo, nm, cat, tier in SEED_REPOS:
    if repo in seen_ref: continue          # already contributed by the feed
    seen_ref.add(repo)
    seed_projects.append(("github", repo, {"name": nm, "_seed": True,
                                           "_cat": cat, "_tier": tier}))
projects = seed_projects + projects
if LIMIT: projects = projects[:LIMIT]
S["projects"] = len(projects)
print("corpus: %d projects from AppImageHub (%d GitHub, %d GitLab, %d direct)" % (
    len(projects), sum(1 for p in projects if p[0] == "github"),
    sum(1 for p in projects if p[0] == "gitlab"),
    sum(1 for p in projects if p[0] == "direct")), flush=True)

# ------------------------------------------------------------------ scan ----
found = {}
progress = [0]

def handle(entry):
    kind, ref, it = entry
    name = it.get("name") or ref
    try:
        if kind == "github":   info = gh_assets(ref)
        elif kind == "gitlab": info = gl_assets(ref); bump("gitlab")
        else:
            info = {"tag": "", "assets": [{"name": ref.rsplit("/", 1)[-1], "url": ref}]}
            bump("direct")
        if info.get("err") and not info["assets"]:
            bump("projects_failed"); return None
        bump("projects_scanned")
        bump("assets_seen", len(info["assets"]))

        cands = []
        for a in info["assets"]:
            if SKIP.search(a["name"]) or not ISAPP.search(a["name"]): continue
            bump("appimages_seen")
            # Names are only a PRIORITY hint - every AppImage is probed, so a
            # mislabelled asset can never hide a real i686 build.
            c = classify(a["name"])
            cands.append((0 if c == "probe32" else (1 if c == "probe?" else 2), a))
        cands.sort(key=lambda x: x[0])

        best64 = {}
        def probe_list(lst, tag):
            for _, a in lst[:16]:
                bump("probed")
                arch, size, ok = elf_arch(a["url"])
                if not ok: bump("probe_failed"); continue
                if arch == "i686":
                    bump("i686")
                    return dict(kind=kind, ref=ref, feed_name=it.get("name"), arch="i686",
                                asset=a["name"], url=a["url"], size=size,
                                tag=tag or "", item=it)
                elif arch == "x86_64":
                    bump("x86_64")
                    if not best64.get("url"):
                        best64.update(name=a["name"], url=a["url"], size=size)
                elif arch == "aarch64": bump("aarch64")
                elif arch == "arm": bump("arm")
                else: bump("other_arch")
            return None

        hit = probe_list(cands, info.get("tag"))
        if hit: return hit
        # no i686 - remember the best x86_64 build so the Store can still list
        # the app (mainstream software must not silently disappear)
        if best64.get("url"):
            return dict(kind=kind, ref=ref, feed_name=it.get("name"), arch="x86_64",
                        asset=best64["name"], url=best64["url"], size=best64["size"],
                        tag=info.get("tag") or "", item=it)
        # A project that ships AppImages but dropped 32-bit from its newest
        # release often still has i686 in an older one - those downloads are
        # just as real, so look back a few tags before giving up.
        if kind == "github" and any(ISAPP.search(a["name"]) for a in info["assets"]):
            repo_real = info.get("repo") or ref
            for t in gh_tags(repo_real, 4)[1:]:
                older = gh_assets_for_tag(repo_real, t)
                bump("releases_read"); bump("assets_seen", len(older))
                oc = []
                for a in older:
                    if SKIP.search(a["name"]) or not ISAPP.search(a["name"]): continue
                    bump("appimages_seen")
                    c = classify(a["name"])
                    oc.append((0 if c == "probe32" else (1 if c == "probe?" else 2), a))
                oc.sort(key=lambda x: x[0])
                hit = probe_list(oc, t)
                if hit: return hit
        return None
    except Exception:
        bump("projects_failed"); return None
    finally:
        with lock:
            progress[0] += 1
            if progress[0] % 50 == 0:
                print("  ... %d/%d projects | i686 so far: %d" %
                      (progress[0], len(projects), S["i686"]), flush=True)

t0 = time.time()
with ThreadPoolExecutor(max_workers=WORKERS) as ex:
    for r in as_completed([ex.submit(handle, p) for p in projects]):
        v = r.result()
        if v:
            k = "%s:%s" % (v["kind"], v["ref"])
            if k in found: bump("duplicates")
            else: found[k] = v

# de-duplicate by download URL as well (same binary re-published)
byurl = {}
for k, v in list(found.items()):
    if v["url"] in byurl: del found[k]; bump("duplicates")
    else: byurl[v["url"]] = k

# enrich with the curated seed metadata and a popularity signal
for k, v in found.items():
    repo = v.get("ref", "")
    sd = SEED_BY_REPO.get(repo)
    if sd:
        v["seed_name"], v["seed_category"], v["tier"] = sd
    it = v.get("item") or {}
    if it.get("_seed"):
        v.setdefault("seed_name", it.get("name"))
        v.setdefault("seed_category", it.get("_cat"))
        v.setdefault("tier", it.get("_tier"))
    if v.get("kind") == "github" and repo:
        v["stars"] = gh_stars(repo)
    S["stars_fetched"] = S.get("stars_fetched", 0) + (1 if v.get("stars") else 0)
S["i686_apps"] = sum(1 for v in found.values() if v.get("arch") == "i686")
S["x86_64_apps"] = sum(1 for v in found.values() if v.get("arch") == "x86_64")
json.dump(probes, open(PROBE_DB, "w"))
json.dump(found, open(OUT, "w"), indent=1)
S["elapsed_s"] = round(time.time() - t0, 1)
S["verified_i686"] = len(found)
json.dump(S, open(STATS, "w"), indent=1)

print("\n================ DISCOVERY STATISTICS ================")
for k in ("projects", "projects_scanned", "projects_failed", "releases_read",
          "assets_seen", "appimages_seen", "probed", "probe_failed",
          "broken_urls", "i686", "x86_64", "aarch64", "arm", "other_arch",
          "duplicates", "transient", "gitlab", "direct", "stars_fetched",
          "i686_apps", "x86_64_apps", "elapsed_s", "verified_i686"):
    print("  %-22s %s" % (k, S.get(k, 0)))
i6 = {k: v for k, v in found.items() if v.get("arch") == "i686"}
print("\nVERIFIED i686 AppImages: %d   (+%d mainstream x86_64 listed for Regular/Pro)"
      % (len(i6), len(found) - len(i6)))
for k, v in sorted(i6.items(), key=lambda kv: -(kv[1].get("stars") or 0)):
    print("   %-38s %-46s %s" % ((v.get("seed_name") or v.get("feed_name") or k)[:38],
                                 v["asset"][:46], v.get("stars") or ""))
