#!/usr/bin/env python3
import os, sys, time, socket, json, subprocess

W, H = 1024, 768
B = "/home/jantzen/xxri-build"
D = f"{B}/work/vm"
SHOTS = f"{D}/shots"
QMP_SOCK = f"{D}/qmp.sock"
MON_SOCK = f"{D}/mon.sock"

class VMTest:
    def __init__(self):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(QMP_SOCK)
        self.f = self.s.makefile("rw", encoding="utf-8", newline="\n")
        self.f.readline()
        self.cmd("qmp_capabilities")

    def cmd(self, execute, **args):
        self.f.write(json.dumps({"execute": execute, "arguments": args} if args else {"execute": execute}) + "\n")
        self.f.flush()
        while True:
            line = self.f.readline()
            if not line: return None
            msg = json.loads(line)
            if "return" in msg or "error" in msg:
                return msg

    def events(self, evs):
        return self.cmd("input-send-event", events=evs)

    def abs_to(self, x, y):
        return [{"type": "abs", "data": {"axis": "x", "value": int(x * 32767 / W)}},
                {"type": "abs", "data": {"axis": "y", "value": int(y * 32767 / H)}}]

    def move(self, x, y):
        self.events(self.abs_to(x, y))
        time.sleep(0.15)

    def button(self, down, btn="left"):
        self.events([{"type": "btn", "data": {"down": down, "button": btn}}])
        time.sleep(0.15)

    def click(self, x, y, btn="left"):
        self.move(x, y)
        time.sleep(0.2)
        self.button(True, btn)
        time.sleep(0.1)
        self.button(False, btn)
        time.sleep(0.5)

    def shot(self, name):
        ppm = f"{D}/s.ppm"
        png = f"{SHOTS}/{name}.png"
        try: os.remove(ppm)
        except OSError: pass
        p = subprocess.Popen(["socat", "-", f"UNIX-CONNECT:{MON_SOCK}"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        p.communicate(f"screendump {ppm}\n".encode())
        time.sleep(0.5)
        subprocess.run(["magick", ppm, png], check=True)
        print(f"Captured: {png}")

def run_tests():
    vm = VMTest()
    vm.shot("01-desktop")

    print("=== TEST 1: SETTINGS WINDOW HELD MOVE ===")
    # Click Settings on dock (gear icon around x=416, y=720)
    print("Clicking Settings icon on dock (x=416, y=720)...")
    vm.click(416, 720)
    time.sleep(3.0)
    vm.shot("settings-01-opened")

    # The settings window starts around x=77, y=60 (w=870, h=620).
    # Draggable header area is around x=280, y=75
    start_x, start_y = 280, 75
    target_x, target_y = 480, 200
    delta_x = target_x - start_x
    delta_y = target_y - start_y

    print(f"Pressing and holding at ({start_x}, {start_y})...")
    vm.move(start_x, start_y)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    # Move intermediate point 1 while STILL HOLDING
    mid1_x = int(start_x + delta_x * 0.35)
    mid1_y = int(start_y + delta_y * 0.35)
    print(f"Moving to intermediate point 1 ({mid1_x}, {mid1_y}) while HOLDING...")
    vm.move(mid1_x, mid1_y)
    time.sleep(0.4)
    vm.shot("settings-02-held-drag-mid1")

    # Move intermediate point 2 while STILL HOLDING
    mid2_x = int(start_x + delta_x * 0.70)
    mid2_y = int(start_y + delta_y * 0.70)
    print(f"Moving to intermediate point 2 ({mid2_x}, {mid2_y}) while HOLDING...")
    vm.move(mid2_x, mid2_y)
    time.sleep(0.4)
    vm.shot("settings-03-held-drag-mid2")

    # Move to target and RELEASE
    print(f"Moving to target ({target_x}, {target_y}) and releasing...")
    vm.move(target_x, target_y)
    time.sleep(0.4)
    vm.button(False, "left")
    time.sleep(0.8)
    vm.shot("settings-04-moved-released")

    # Now click visible controls at the NEW window position
    # The whole window moved by (delta_x, delta_y) = (+200, +125)
    # Original Storage button in sidebar: x ~ 230, y ~ 615 -> new position x=430, y=740? Dock is at bottom, let's click Bluetooth or Wallpaper & Style!
    # Original "Wallpaper & Style": x ~ 230, y ~ 455 -> new position x=430, y=580.
    print("Clicking 'Wallpaper & Style' at NEW position (x=430, y=580)...")
    vm.click(430, 580)
    time.sleep(1.0)
    vm.shot("settings-05-clicked-wallpaper-new-pos")

    # Now click the close control at the NEW position
    # Original close button: x ~ 190, y ~ 110 (or top left glyph).
    # Let's check close button coordinates in Settings:
    # Sidebar top-left controls:
    # XC_MIN is x=8, XC_MAX is x=23, XC_CLOSE is x=38 relative to da (which is placed in sidebar header at window_x + 80..100, window_y + 40..50).
    # In 00-boot settings, close button is at ~ x=190, y=110.
    # After moving by (+200, +125): close button is at x=390, y=235.
    print("Clicking close control at NEW position (x=390, y=235)...")
    vm.click(390, 235)
    time.sleep(1.5)
    vm.shot("settings-06-closed")

    print("=== TEST 2: STORE WINDOW HELD MOVE ===")
    # Click Store on dock (shopping bag icon around x=375, y=720)
    print("Clicking Store icon on dock (x=375, y=720)...")
    vm.click(375, 720)
    time.sleep(3.5)
    vm.shot("store-01-opened")

    # Store header draggable area around x=350, y=85
    start_x, start_y = 350, 85
    target_x, target_y = 470, 160
    delta_x = target_x - start_x
    delta_y = target_y - start_y

    print(f"Pressing and holding at ({start_x}, {start_y})...")
    vm.move(start_x, start_y)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    # Move intermediate point while STILL HOLDING
    mid1_x = int(start_x + delta_x * 0.5)
    mid1_y = int(start_y + delta_y * 0.5)
    print(f"Moving to intermediate point ({mid1_x}, {mid1_y}) while HOLDING...")
    vm.move(mid1_x, mid1_y)
    time.sleep(0.4)
    vm.shot("store-02-held-drag-mid")

    # Move to target and RELEASE
    print(f"Moving to target ({target_x}, {target_y}) and releasing...")
    vm.move(target_x, target_y)
    time.sleep(0.4)
    vm.button(False, "left")
    time.sleep(0.8)
    vm.shot("store-03-moved-released")

    # Click visible control at NEW position: "Refresh" button
    # Original "Refresh" is around x=495, y=125 -> new position x=495+120=615, y=125+75=200
    print("Clicking 'Refresh' at NEW position (x=615, y=200)...")
    vm.click(615, 200)
    time.sleep(1.0)
    vm.shot("store-04-clicked-refresh-new-pos")

    # Click close button at NEW position
    # In Store, close button is around x=125, y=110 -> new position x=125+120=245, y=110+75=185
    print("Clicking close control at NEW position (x=245, y=185)...")
    vm.click(245, 185)
    time.sleep(1.5)
    vm.shot("store-05-closed")

    print("ALL TESTS COMPLETED!")

if __name__ == "__main__":
    run_tests()
