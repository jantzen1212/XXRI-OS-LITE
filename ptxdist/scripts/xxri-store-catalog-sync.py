#!/usr/bin/env python3
"""xxri-store-catalog-sync.py OUTDIR

Fetch the x86_64 Store catalog, validate it, and leave OUTDIR/x64.json plus
OUTDIR/x64.json.source (where it came from) for the xxri-store-catalog package.
Only the catalog is fetched, never the AppImages.

  default                       https://repo.xxri.flows.best/x64.json, the URL
                                the Store itself refreshes from
  XXRI_STORE_CATALOG_URL=<url>  another URL; file:///path/x64.json for an
                                offline build from a copy on disk

The server's ETag is kept next to the copy, so an unchanged catalog is answered
with 304 and not downloaded again; OUTDIR is only rewritten when the catalog
changed, so an unchanged catalog does not repackage anything.

It fails, loudly, when the catalog cannot be fetched or does not pass the checks
below. It never substitutes another file (i686.json in particular) and never
keeps an older copy when the fetch fails.

The checks are what XXRI Store reads: schema 3.x, a non-empty applications list,
ids that are safe to put on a command line and unique, every entry of architecture
x86_64 (the nested "sources" objects too), the string fields the Store shows or
fetches, a size that is an integer, a checksum that is a SHA-256 or null.
"""
import hashlib, json, os, re, ssl, sys, urllib.error, urllib.request

URL = "https://repo.xxri.flows.best/x64.json"
FILE = "x64.json"
ARCH = "x86_64"
LIMIT = 20 * 1024 * 1024
ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._+-]*$")
# PTXdist's own Python looks for certificates under its sysroot, which has none
CA_FILES = ("/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
            "/etc/ssl/ca-bundle.pem", "/etc/ssl/cert.pem")


def say(msg):
    print("xxri-store-catalog: " + msg, flush=True)


def die(msg, hint=None):
    print("xxri-store-catalog: ERROR: " + msg, file=sys.stderr)
    if hint:
        print("xxri-store-catalog: " + hint, file=sys.stderr)
    sys.exit(1)


def tls_context():
    ctx = ssl.create_default_context()
    if os.environ.get("SSL_CERT_FILE") or ctx.cert_store_stats()["x509_ca"]:
        return ctx
    for f in CA_FILES:
        if os.path.isfile(f):
            return ssl.create_default_context(cafile=f)
    return ctx


def fetch(url, etag):
    """-> (body, headers), or (None, headers) when the server says 304"""
    req = urllib.request.Request(url, headers={"User-Agent": "xxri-store-catalog-sync"})
    if etag:
        req.add_header("If-None-Match", etag)
    try:
        with urllib.request.urlopen(req, timeout=60, context=tls_context()) as r:
            data = r.read(LIMIT + 1)
            headers = r.headers
    except urllib.error.HTTPError as e:
        if e.code == 304:
            return None, e.headers
        raise
    if len(data) > LIMIT:
        raise ValueError("answer is larger than %d MiB" % (LIMIT >> 20))
    return data, headers


def validate(raw):
    """-> (document, applications); raises ValueError naming what is wrong"""
    try:
        doc = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        raise ValueError("not valid JSON: %s" % e)
    if not isinstance(doc, dict):
        raise ValueError("the top level is not an object")
    if not str(doc.get("schema_version", "")).startswith("3."):
        raise ValueError("schema_version is %r, the Store reads 3.x" % doc.get("schema_version"))
    if not re.match(r"^\d{4}-\d{2}-\d{2}$", str(doc.get("last_updated", ""))):
        raise ValueError("last_updated is %r, not a date" % doc.get("last_updated"))
    apps = doc.get("applications")
    if not isinstance(apps, list) or not apps:
        raise ValueError("applications is missing or empty")
    seen = set()
    for n, a in enumerate(apps):
        who = "entry %d" % n
        if not isinstance(a, dict):
            raise ValueError("%s is not an object" % who)
        aid = a.get("id")
        if not isinstance(aid, str) or not ID_RE.match(aid):
            raise ValueError("%s: id %r is not a plain name" % (who, aid))
        if aid in seen:
            raise ValueError("id %r appears twice" % aid)
        seen.add(aid)
        who = "'%s'" % aid
        for k in ("title", "category", "version", "download_url", "package_type", "architecture"):
            if not isinstance(a.get(k), str) or not a[k].strip():
                raise ValueError("%s: %s is missing or empty" % (who, k))
        if a["architecture"] != ARCH:
            raise ValueError("%s: architecture is %r, this is the %s catalog" % (who, a["architecture"], ARCH))
        if not re.match(r"^https?://\S+$", a["download_url"]):
            raise ValueError("%s: download_url %r is not an http(s) URL" % (who, a["download_url"]))
        sz = a.get("size")
        if isinstance(sz, bool) or not isinstance(sz, int) or sz < 0:
            raise ValueError("%s: size %r is not a non-negative integer" % (who, sz))
        ck = a.get("checksum")
        if ck is not None and not (isinstance(ck, str) and re.match(r"^[0-9a-fA-F]{64}$", ck)):
            raise ValueError("%s: checksum %r is not a SHA-256" % (who, ck))
        for k in ("icon_url", "description", "homepage"):
            if k in a and not isinstance(a[k], str):
                raise ValueError("%s: %s is not a string" % (who, k))
        if "verified" in a and not isinstance(a["verified"], bool):
            raise ValueError("%s: verified is not a boolean" % who)
        for s in a.get("sources") or []:
            if isinstance(s, dict) and "architecture" in s and s["architecture"] != ARCH:
                raise ValueError("%s: a source is for %r" % (who, s["architecture"]))
    return doc, apps


def write_if_changed(path, data):
    try:
        if open(path, "rb").read() == data:
            return False
    except OSError:
        pass
    tmp = path + ".new"
    open(tmp, "wb").write(data)
    os.replace(tmp, path)
    return True


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    url = os.environ.get("XXRI_STORE_CATALOG_URL") or URL
    dest, src_file = os.path.join(out, FILE), os.path.join(out, FILE + ".source")
    try:
        prev = json.load(open(src_file))
    except (OSError, ValueError):
        prev = {}
    # the ETag only counts for the same URL and when the copy it describes is there
    etag = prev.get("etag") if prev.get("url") == url and os.path.isfile(dest) else None
    try:
        raw, headers = fetch(url, etag)
    except (urllib.error.URLError, OSError, ValueError) as e:
        die("could not fetch %s: %s" % (url, e),
            "The catalog is required for the 64-bit image; nothing else is packaged in its place. "
            "Offline: XXRI_STORE_CATALOG_URL=file:///path/to/x64.json")
    if raw is None:
        say("%s: not modified (ETag %s)" % (url, etag))
        raw, headers = open(dest, "rb").read(), {"ETag": etag, "Last-Modified": prev.get("last_modified")}
    try:
        doc, apps = validate(raw)
    except ValueError as e:
        die("%s does not pass the checks: %s" % (url, e),
            "Nothing was packaged. Fix x64.json in the repository.")
    source = {   # no build time: the same catalog gives the same bytes
        "catalog": FILE, "architecture": ARCH, "url": url,
        "etag": headers.get("ETag") or etag, "last_modified": headers.get("Last-Modified"),
        "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw),
        "applications": len(apps), "schema_version": doc["schema_version"], "last_updated": doc["last_updated"],
    }
    os.makedirs(out, exist_ok=True)
    c1 = write_if_changed(dest, raw)
    c2 = write_if_changed(src_file, (json.dumps(source, indent=2) + "\n").encode())
    nodigest = sum(1 for a in apps if a.get("checksum") is None)
    say("%s: %d applications (%d without a pinned checksum), schema %s, last_updated %s, sha256 %s" %
        (url, len(apps), nodigest, doc["schema_version"], doc["last_updated"], source["sha256"]))
    say("packaged copy updated" if c1 or c2 else "packaged copy already current")


if __name__ == "__main__":
    main()
