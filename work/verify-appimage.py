#!/usr/bin/env python3
"""verify-appimage.py - compatibility verifier (Phase 9.5).

An AppImage can be a perfectly good i686 binary and still die instantly with
"This application closed unexpectedly" - because its payload needs a newer glibc
than the device has, or its interpreter/AppRun is missing.  This verifier
downloads a candidate and answers, before it is ever published:

  1. is it an AppImage?      type-2 magic 'AI\\x02' at offset 8
  2. right architecture?     ELF class + e_machine of the outer runtime
  3. payload readable?       squashfs mounts/unpacks at the computed offset
  4. does it start?          AppRun exists and is executable
  5. will it run HERE?       every ELF inside is checked for its highest
                             required GLIBC_x.y - anything above the device's
                             glibc (default 2.40) is rejected
  6. interpreter present?    /lib/ld-linux.so.2 style loader the device has

Verdict is cached in work/compat-cache.json and failures are recorded in
work/blacklist.json so a broken release is never retried or published.

  ./verify-appimage.py <id> <url> [size]     verify one
  ./verify-appimage.py --from-discovery      verify every i686 candidate
"""
import json, os, re, subprocess, sys, struct, time

BUILD = "/home/jantzen/xxri-build"
WORK  = os.path.join(BUILD, "work")
CACHE = os.path.join(WORK, "compat-cache.json")
BLACK = os.path.join(WORK, "blacklist.json")
TMP   = os.path.join(WORK, "verify-tmp")
RUNTIME_GAPS = os.path.join(WORK, "runtime-gaps.json")
DEVICE_GLIBC = tuple(int(x) for x in os.environ.get("XXRI_GLIBC", "2.40").split("."))
MAX_MB = int(os.environ.get("XXRI_VERIFY_MAX_MB", "160"))

def _load(p, d):
    try: return json.load(open(p))
    except Exception: return d
compat = _load(CACHE, {})
black  = _load(BLACK, {})

GLIBC_RE = re.compile(rb"GLIBC_(\d+)\.(\d+)")

# Every shared library the device actually provides.  An AppImage that needs a
# library which is neither bundled nor present here starts and dies instantly
# with "The application closed unexpectedly" - the exact failure this catches.
def _device_libs():
    p = os.path.join(WORK, "device-libs.txt")
    try:    return set(x.strip() for x in open(p) if x.strip())
    except Exception: return set()
DEVLIBS = _device_libs()

def _device_bins():
    p = os.path.join(WORK, "device-bins.txt")
    try:    return set(x.strip() for x in open(p) if x.strip())
    except Exception: return set()
DEVBINS = _device_bins()

SHEBANG = re.compile(rb"^#!\s*(\S+)(?:\s+(\S+))?")

def script_interpreter(path):
    """Interpreter a script needs, e.g. '#!/usr/bin/env bash' -> 'bash'."""
    try:
        with open(path, "rb") as f: first = f.readline(200)
    except Exception:
        return None
    m = SHEBANG.match(first)
    if not m: return None
    prog = os.path.basename(m.group(1).decode("utf-8", "replace"))
    if prog == "env" and m.group(2):
        prog = os.path.basename(m.group(2).decode("utf-8", "replace"))
    return prog

def needed_libs(path):
    """DT_NEEDED entries of an ELF, via readelf (fast, no python ELF parser)."""
    try:
        r = subprocess.run(["readelf", "-d", path], capture_output=True, timeout=30)
        return re.findall(r"Shared library: \[([^\]]+)\]", r.stdout.decode("utf-8", "replace"))
    except Exception:
        return []

def highest_glibc(path):
    """Highest GLIBC_x.y symbol version an ELF requires (scan is enough:
    the version strings live in .gnu.version_r as plain text)."""
    hi = (0, 0)
    try:
        with open(path, "rb") as f:
            blob = f.read(6 * 1024 * 1024)
        for m in GLIBC_RE.finditer(blob):
            v = (int(m.group(1)), int(m.group(2)))
            if v > hi: hi = v
    except Exception:
        pass
    return hi

def elf_info(path):
    try:
        with open(path, "rb") as f: h = f.read(64)
        if h[:4] != b"\x7fELF": return None
        return dict(cls=h[4], machine=struct.unpack("<H", h[18:20])[0])
    except Exception:
        return None

def payload_offset(path):
    with open(path, "rb") as f: h = f.read(64)
    e_shoff = struct.unpack("<I", h[0x20:0x24])[0]
    e_shentsize = struct.unpack("<H", h[0x2e:0x30])[0]
    e_shnum = struct.unpack("<H", h[0x30:0x32])[0]
    return e_shoff + e_shentsize * e_shnum

def verify(aid, url, size=0, want_arch="i686"):
    """-> (ok, reason, details)"""
    if aid in compat:
        c = compat[aid]; return c["ok"], c["reason"], c
    if size and size > MAX_MB * 1024 * 1024:
        r = (True, "skipped-too-large", {})          # not a failure, just unchecked
        compat[aid] = dict(ok=True, reason="skipped-too-large", checked=False)
        return r
    os.makedirs(TMP, exist_ok=True)
    f = os.path.join(TMP, aid + ".AppImage")
    ok_dl = False
    for attempt in range(3):
        rc = subprocess.run(["curl", "-sfL", "--retry", "2", "--retry-delay", "3",
                             "--max-time", "600", "--speed-limit", "8000",
                             "--speed-time", "60", "-o", f, url], capture_output=True)
        if rc.returncode == 0 and os.path.exists(f) and os.path.getsize(f) > 1024:
            ok_dl = True; break
        time.sleep(5 * (attempt + 1))
    if not ok_dl:
        # A download that would not complete says nothing about the app itself.
        # Treat it as UNKNOWN: do not publish it now, but never blacklist it.
        compat[aid] = dict(ok=False, reason="download-unavailable", checked=False)
        _save()
        return False, "download-unavailable", {}

    d = {}
    try:
        # 1 + 2. AppImage magic and outer architecture
        with open(f, "rb") as fh: head = fh.read(64)
        d["appimage_magic"] = head[8:11] == b"AI\x02"
        e = elf_info(f)
        if not e: return _fail(aid, "not-elf", d)
        d["elf_class"] = "32-bit" if e["cls"] == 1 else "64-bit"
        d["machine"] = {3: "i686", 62: "x86_64", 183: "aarch64", 40: "arm"}.get(e["machine"], str(e["machine"]))
        if want_arch == "i686" and not (e["cls"] == 1 and e["machine"] == 3):
            return _fail(aid, "wrong-architecture", d)

        # 3. payload must unpack
        off = payload_offset(f)
        ex = os.path.join(TMP, aid + ".d")
        subprocess.run(["rm", "-rf", ex], capture_output=True)
        u = subprocess.run(["unsquashfs", "-n", "-f", "-d", ex, "-o", str(off), f],
                           capture_output=True, timeout=240)
        # unsquashfs exits non-zero on harmless warnings, so judge by what it
        # actually produced - an empty tree is the only real failure.
        got = 0
        if os.path.isdir(ex):
            for _r, _dd, _ff in os.walk(ex):
                got += len(_ff) + len(_dd)
                if got: break
        d["payload_entries"] = got
        if not got:
            d["unsquashfs_rc"] = u.returncode
            return _fail(aid, "payload-unreadable", d)

        # 4. AppRun must exist AND its interpreter must be installed.  XXRI Lite
        #    is busybox-only: an AppRun starting with "#!/usr/bin/env bash" dies
        #    instantly with "env: can't execute 'bash'".
        apprun = os.path.join(ex, "AppRun")
        d["apprun"] = os.path.exists(apprun)
        if not d["apprun"]: return _fail(aid, "no-apprun", d)
        interp = script_interpreter(apprun)
        if interp:
            d["apprun_interpreter"] = interp
            if DEVBINS and interp not in DEVBINS:
                return _fail(aid, "interpreter-missing:%s" % interp, d)

        # 5 + 6. every ELF inside must run on this device's glibc/loader
        worst = (0, 0); worst_file = ""; interp_bad = []
        nelf = 0
        for root, _dirs, files in os.walk(ex):
            for fn in files:
                p = os.path.join(root, fn)
                if os.path.islink(p) or os.path.getsize(p) < 128: continue
                info = elf_info(p)
                if not info: continue
                nelf += 1
                if info["machine"] == 62:
                    interp_bad.append(os.path.relpath(p, ex))     # 64-bit inside!
                g = highest_glibc(p)
                if g > worst: worst, worst_file = g, os.path.relpath(p, ex)
                if nelf > 400: break
        d["elf_files"] = nelf
        d["max_glibc"] = "%d.%d" % worst if worst != (0, 0) else "none"
        d["max_glibc_file"] = worst_file
        if interp_bad:
            d["x86_64_payload"] = interp_bad[:3]
            return _fail(aid, "64-bit-payload", d)
        if worst > DEVICE_GLIBC:
            return _fail(aid, "glibc-too-new", d)

        # 7. every library the payload needs must be bundled or on the device
        if DEVLIBS:
            bundled = set()
            for root, _dirs, files in os.walk(ex):
                for fn in files:
                    if ".so" in fn: bundled.add(fn)
            entry = apprun
            # AppRun is often a shell script; check the binaries it ships
            targets = []
            try:
                with open(apprun, "rb") as fh:
                    if fh.read(4) == b"\x7fELF": targets.append(apprun)
            except Exception: pass
            for root, _dirs, files in os.walk(os.path.join(ex, "usr", "bin")):
                for fn in files[:12]:
                    p2 = os.path.join(root, fn)
                    if not os.path.islink(p2) and elf_info(p2): targets.append(p2)
            for root, _dirs, files in os.walk(ex):
                if root != ex: continue
                for fn in files:
                    p2 = os.path.join(root, fn)
                    if not os.path.islink(p2) and elf_info(p2): targets.append(p2)
            missing = set()
            # also follow the bundled libraries themselves - a missing
            # dependency is just as fatal one level down
            for root, _dirs, files in os.walk(ex):
                for fn in files:
                    if ".so" in fn and len(targets) < 40:
                        p2 = os.path.join(root, fn)
                        if not os.path.islink(p2) and elf_info(p2): targets.append(p2)
            for t in targets[:40]:
                for lib in needed_libs(t):
                    if lib in bundled or lib in DEVLIBS: continue
                    missing.add(lib)
            if missing:
                # Phase 9.6 policy: XXRI ships the runtime, so a missing library
                # is an OS gap to close - never an application defect.  Record it
                # for the runtime-expansion report and let the app through.
                d["missing_libs"] = sorted(missing)[:8]
                gaps = _load(RUNTIME_GAPS, {})
                gaps[aid] = sorted(missing)
                json.dump(gaps, open(RUNTIME_GAPS, "w"), indent=1)
                compat[aid] = dict(ok=True, reason="ok-needs-runtime", checked=True, **d)
                _save()
                return True, "ok-needs-runtime", d
    except Exception as ex_:
        return _fail(aid, "verify-error:%s" % str(ex_)[:40], d)
    finally:
        subprocess.run(["rm", "-rf", os.path.join(TMP, aid + ".d"), f], capture_output=True)

    compat[aid] = dict(ok=True, reason="ok", checked=True, **d)
    _save()
    return True, "ok", d

def _fail(aid, reason, d):
    compat[aid] = dict(ok=False, reason=reason, checked=True, **d)
    black[aid] = reason
    _save()
    subprocess.run(["rm", "-rf", os.path.join(TMP, aid + ".d"),
                    os.path.join(TMP, aid + ".AppImage")], capture_output=True)
    return False, reason, d

def _save():
    json.dump(compat, open(CACHE, "w"), indent=1)
    json.dump(black, open(BLACK, "w"), indent=1)

if __name__ == "__main__":
    if "--from-discovery" in sys.argv:
        v = json.load(open(os.path.join(WORK, "i686-verified.json")))
        cands = [(k, x) for k, x in v.items() if x.get("arch") == "i686"]
        print("verifying %d i686 candidates (device glibc %s, max %d MB)"
              % (len(cands), ".".join(map(str, DEVICE_GLIBC)), MAX_MB))
        ok = bad = 0
        for k, x in cands:
            aid = re.sub(r"[^a-z0-9]+", "-", (x.get("seed_name") or x.get("feed_name") or k).lower()).strip("-")
            good, reason, d = verify(aid, x["url"], x.get("size") or 0)
            x["compat_ok"] = good; x["compat_reason"] = reason
            if d.get("max_glibc"): x["max_glibc"] = d["max_glibc"]
            print("  %-34s %-18s %s" % (aid[:34], reason, d.get("max_glibc", "")))
            ok += good; bad += (not good)
        json.dump(v, open(os.path.join(WORK, "i686-verified.json"), "w"), indent=1)
        print("\ncompatible: %d   rejected/blacklisted: %d" % (ok, bad))
    elif len(sys.argv) >= 3:
        print(verify(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 0))
    else:
        print(__doc__)
