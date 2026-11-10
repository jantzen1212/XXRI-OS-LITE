#!/usr/bin/env python3
"""ptrtest.py NAME [--entry N] [--ps2] [--dev DEVICE] [--bin qemu-system-i386] [--iso ISO]
Boot the ISO and prove the pointer: visible, tracks absolute (tablet) or
relative (ps2) motion, and a click lands. Cursor found by image difference."""
import argparse, json, os, socket, subprocess, sys, time
from PIL import Image, ImageChops
import numpy as np
ap = argparse.ArgumentParser()
ap.add_argument("name"); ap.add_argument("--entry", type=int, default=0)
ap.add_argument("--ps2", action="store_true"); ap.add_argument("--dev", default="VGA")
ap.add_argument("--bin", default="qemu-system-i386"); ap.add_argument("--iso", default=os.environ.get("XXRI_ISO", "XXRI-Lite-2.6.iso"))
ap.add_argument("--extra", default=""); ap.add_argument("--boot", type=int, default=70)
a = ap.parse_args()
D = os.environ.get("PTR_OUT", "/tmp/ptrtest"); os.makedirs(D, exist_ok=True); mon = f"{D}/{a.name}-mon.sock"; qmp = f"{D}/{a.name}-qmp.sock"
for p in (mon, qmp):
    try: os.unlink(p)
    except FileNotFoundError: pass
cmd = [a.bin, "-enable-kvm", "-m", "768", "-cdrom", a.iso, "-boot", "d", "-snapshot", "-vga", "none", "-device", a.dev,
       "-display", "none", "-monitor", f"unix:{mon},server,nowait", "-qmp", f"unix:{qmp},server,nowait", "-usb"]
if not a.ps2: cmd += ["-device", "usb-tablet"]
cmd += a.extra.split()
q = subprocess.Popen(["choom", "-n", "1000", "--"] + cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def sock(p):
    for _ in range(100):
        try:
            s = socket.socket(socket.AF_UNIX); s.connect(p); return s
        except OSError: time.sleep(0.2)
    raise SystemExit("no socket")
M = sock(mon); M.settimeout(3)
Q = sock(qmp); qf = Q.makefile("rw"); qf.readline()
def qcmd(x, **kw):
    qf.write(json.dumps({"execute": x, "arguments": kw}) + "\n"); qf.flush()
    while True:
        l = json.loads(qf.readline())
        if "return" in l or "error" in l: return l
qcmd("qmp_capabilities")
def hmp(c):
    M.sendall((c + "\n").encode()); time.sleep(0.15)
    try: M.recv(65536)
    except Exception: pass
def shot(n):
    f = f"{D}/{a.name}-{n}.ppm"; hmp(f"screendump {f}"); time.sleep(0.6)
    im = Image.open(f).convert("RGB"); os.unlink(f); return im
def key(*k):
    qcmd("send-key", keys=[{"type": "qcode", "data": x} for x in k])
def key_wait(sec):
    time.sleep(sec)
def abs_move(x, y, W, H):
    qcmd("input-send-event", events=[{"type": "abs", "data": {"axis": "x", "value": int(x * 32767 / W)}},
                                     {"type": "abs", "data": {"axis": "y", "value": int(y * 32767 / H)}}])
def click(btn="left"):
    qcmd("input-send-event", events=[{"type": "btn", "data": {"down": True, "button": btn}}]); time.sleep(0.1)
    qcmd("input-send-event", events=[{"type": "btn", "data": {"down": False, "button": btn}}])
def find_cursor(before, after):
    d = np.asarray(ImageChops.difference(before, after).convert("L")) > 40
    ys, xs = np.nonzero(d)
    return None if len(xs) == 0 else (xs.min(), ys.min(), xs.max(), ys.max(), len(xs))
res = {"name": a.name}
try:
    # GRUB menu: pick the entry
    time.sleep(3)
    for _ in range(a.entry): key("down"); time.sleep(0.2)
    key("ret")
    time.sleep(a.boot)
    img = shot("boot"); W, H = img.size; res["screen"] = f"{W}x{H}"
    # the installer is up when the picture is mostly the wallpaper; give it a moment more
    time.sleep(3)
    tests = [(200, 150), (700, 250), (300, 600)]
    ok = 0; res["moves"] = []
    if not a.ps2:
        base = None
        for (x, y) in tests:
            abs_move(5, 5, W, H); time.sleep(1.0); a0 = shot("a0")
            abs_move(x, y, W, H); time.sleep(1.0); a1 = shot("a1")
            bb = find_cursor(a0, a1)   # difference = old cursor at (5,5) + new cursor
            # new cursor = bbox part away from (5,5) corner
            d = np.asarray(ImageChops.difference(a0, a1).convert("L")) > 40
            d[:40, :40] = False
            ys, xs = np.nonzero(d)
            if len(xs) == 0: res["moves"].append({"want": (x, y), "got": None}); continue
            got = (int(xs.min()), int(ys.min()))
            err = max(abs(got[0] - x), abs(got[1] - y))
            res["moves"].append({"want": (x, y), "got": got, "err": err})
            if err <= 12: ok += 1
        res["tracking_ok"] = f"{ok}/{len(tests)}"
    else:
        a0 = shot("a0"); hmp("mouse_move 120 90"); time.sleep(1.0); a1 = shot("a1")
        bb = find_cursor(a0, a1); res["rel_diff"] = None if bb is None else [int(v) for v in bb]
        res["tracking_ok"] = "moved" if bb else "NOT MOVED"
    # click test: the Continue button on the welcome page is at the lower middle; click and expect a page change
    pre = shot("pre")
    if not a.ps2:
        abs_move(W // 2, int(H * 0.738), W, H)
    else:
        hmp("mouse_move 0 0")
    time.sleep(0.6); click(); time.sleep(2.5)
    post = shot("post")
    diff = np.asarray(ImageChops.difference(pre, post).convert("L")) > 40
    res["click_changed_px"] = int(diff.sum())
    post.save(f"{D}/{a.name}-final.png"); pre.save(f"{D}/{a.name}-pre.png")
finally:
    try: hmp("quit")
    except Exception: pass
    q.wait(timeout=10) if q.poll() is None else None
print(json.dumps(res))
