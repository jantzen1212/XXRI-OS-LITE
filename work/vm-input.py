#!/usr/bin/env python3
"""vm-input.py - drive the guest with REAL pointer/keyboard events over QMP.

The HMP `mouse_move` path never reached this guest (relative PS/2 motion the X
server ignored).  With `-device usb-tablet` the guest gets an ABSOLUTE pointer
and QMP `input-send-event` delivers clicks and drags that X.Org acts on, which
is what makes titlebar interaction testable instead of merely photographable.

  vm-input.py SOCK move X Y
  vm-input.py SOCK click X Y [left|right|middle]
  vm-input.py SOCK drag X0 Y0 X1 Y1
  vm-input.py SOCK key KEYNAME...
"""
import json, socket, sys, time

W, H = 1024, 768          # override with XXRI_RES=WxH


class QMP:
    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)
        self.f = self.s.makefile("rw", encoding="utf-8", newline="\n")
        self.f.readline()                      # greeting
        self.cmd("qmp_capabilities")

    def cmd(self, execute, **args):
        self.f.write(json.dumps({"execute": execute, "arguments": args} if args
                                else {"execute": execute}) + "\n")
        self.f.flush()
        while True:                            # skip async events
            line = self.f.readline()
            if not line:
                return None
            msg = json.loads(line)
            if "return" in msg or "error" in msg:
                return msg

    def events(self, evs):
        return self.cmd("input-send-event", events=evs)

    def abs_to(self, x, y):
        return [{"type": "abs", "data": {"axis": "x", "value": int(x * 32767 / W)}},
                {"type": "abs", "data": {"axis": "y", "value": int(y * 32767 / H)}}]

    def move(self, x, y):
        self.events(self.abs_to(x, y)); time.sleep(0.25)

    def button(self, down, btn="left"):
        self.events([{"type": "btn", "data": {"down": down, "button": btn}}])
        time.sleep(0.15)

    def click(self, x, y, btn="left"):
        self.move(x, y); time.sleep(0.3)
        self.button(True, btn); time.sleep(0.12); self.button(False, btn)
        time.sleep(0.6)

    def drag(self, x0, y0, x1, y1, steps=14):
        self.move(x0, y0); time.sleep(0.4)
        self.button(True); time.sleep(0.3)
        for i in range(1, steps + 1):          # many small steps: a single jump
            x = x0 + (x1 - x0) * i / steps     # can be swallowed as a stray warp
            y = y0 + (y1 - y0) * i / steps
            self.events(self.abs_to(x, y)); time.sleep(0.06)
        time.sleep(0.35); self.button(False); time.sleep(0.7)

    def key(self, *names):
        self.cmd("send-key", keys=[{"type": "qcode", "data": n} for n in names])
        time.sleep(0.3)


def main():
    global W, H
    import os
    if "XXRI_RES" in os.environ:
        W, H = (int(v) for v in os.environ["XXRI_RES"].split("x"))
    sock, op, *rest = sys.argv[1:]
    q = QMP(sock)
    if op == "move":
        q.move(int(rest[0]), int(rest[1]))
    elif op == "click":
        q.click(int(rest[0]), int(rest[1]), rest[2] if len(rest) > 2 else "left")
    elif op == "press":
        q.move(int(rest[0]), int(rest[1])); q.button(True)
    elif op == "release":
        if rest: q.move(int(rest[0]), int(rest[1]))
        q.button(False)
    elif op == "drag":
        q.drag(*(int(v) for v in rest[:4]))
    elif op == "key":
        q.key(*rest)
    else:
        sys.exit("unknown op " + op)
    print(f"OK {op} {' '.join(rest)}")


main()
