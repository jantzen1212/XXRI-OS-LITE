#!/usr/bin/env python3
import os, sys, time, socket, json, subprocess

W, H = 1024, 768
B = "/home/jantzen/xxri-build"
D = f"{B}/work/vm"
SHOTS = f"{D}/shots"
QMP_SOCK = f"{D}/qmp.sock"
MON_SOCK = f"{D}/mon.sock"

class VMDriver:
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
        time.sleep(0.12)

    def button(self, down, btn="left"):
        self.events([{"type": "btn", "data": {"down": down, "button": btn}}])
        time.sleep(0.12)

    def click(self, x, y, btn="left"):
        self.move(x, y)
        time.sleep(0.2)
        self.button(True, btn)
        time.sleep(0.12)
        self.button(False, btn)
        time.sleep(0.5)

    def shot(self, name):
        ppm = f"{D}/s.ppm"
        png = f"{SHOTS}/{name}.png"
        try: os.remove(ppm)
        except OSError: pass
        p = subprocess.Popen(["socat", "-", f"UNIX-CONNECT:{MON_SOCK}"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        p.communicate(f"screendump {ppm}\n".encode())
        time.sleep(0.4)
        subprocess.run(["magick", ppm, png], check=True)
        print(f"Captured: {png}")
        return ppm

def find_controls_in_ppm(ppm_path):
    with open(ppm_path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6":
            return None
        # skip comments
        line = f.readline()
        while line.startswith(b"#"):
            line = f.readline()
        dims = line.split()
        width, height = int(dims[0]), int(dims[1])
        maxval = int(f.readline().strip())
        raw_data = f.read()

    # Search top-down for the magenta minimize glyph (RGB ~ 230, 36, 206)
    # in PPM format: bytes are R, G, B for each pixel
    for y in range(height):
        row_offset = y * width * 3
        for x in range(width):
            idx = row_offset + x * 3
            r, g, b = raw_data[idx], raw_data[idx+1], raw_data[idx+2]
            if 210 <= r <= 245 and 20 <= g <= 50 and 190 <= b <= 220:
                return x, y
    return None

def main():
    vm = VMDriver()

    print("==================================================")
    print("STEP 1: Testing Settings Window HELD-BUTTON Move")
    print("==================================================")

    # 1. Click Settings on dock (x=440, y=719)
    print("Launching Settings...")
    vm.click(440, 719)
    time.sleep(8.0)
    ppm_opened = vm.shot("settings-01-opened")

    controls = find_controls_in_ppm(ppm_opened)
    print(f"Settings controls located at: {controls}")
    if not controls:
        print("ERROR: Could not locate Settings window controls!")
        sys.exit(1)

    min_x, min_y = controls

    # Header drag area: ~120px right of controls, same y
    drag_start_x = min_x + 120
    drag_start_y = min_y + 10

    drag_target_x = drag_start_x + 180
    drag_target_y = drag_start_y + 120
    total_dx = drag_target_x - drag_start_x
    total_dy = drag_target_y - drag_start_y

    print(f"Pressing and HOLDING mouse at ({drag_start_x}, {drag_start_y})...")
    vm.move(drag_start_x, drag_start_y)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    # Drag in steps, capturing intermediate frames WHILE STILL HOLDING
    for i, frac in enumerate([0.25, 0.50, 0.75, 1.0], 1):
        cur_x = int(drag_start_x + total_dx * frac)
        cur_y = int(drag_start_y + total_dy * frac)
        print(f"  Holding & dragging to intermediate step {i}/4: ({cur_x}, {cur_y})...")
        vm.move(cur_x, cur_y)
        time.sleep(0.3)
        vm.shot(f"settings-02-held-frame-{i}")

    print("Releasing mouse button at final position...")
    vm.button(False, "left")
    time.sleep(0.8)
    ppm_moved = vm.shot("settings-03-moved-released")

    new_controls = find_controls_in_ppm(ppm_moved)
    print(f"Settings controls at NEW position: {new_controls}")
    if not new_controls:
        print("ERROR: Controls not found after move!")
        sys.exit(1)

    new_min_x, new_min_y = new_controls
    print(f"Shift detected: dx={new_min_x - min_x}, dy={new_min_y - min_y}")

    # Click visible control at NEW position: "Wallpaper & Style"
    wp_x = new_min_x + 40
    wp_y = new_min_y + 220
    print(f"Clicking 'Wallpaper & Style' at NEW position ({wp_x}, {wp_y})...")
    vm.click(wp_x, wp_y)
    time.sleep(1.2)
    vm.shot("settings-04-clicked-new-control")

    # Click close button at NEW position (XC_CLOSE is ~30px right of XC_MIN)
    new_close_x = new_min_x + 30
    new_close_y = new_min_y
    print(f"Clicking close control at NEW position ({new_close_x}, {new_close_y})...")
    vm.click(new_close_x, new_close_y)
    time.sleep(2.0)
    vm.shot("settings-05-closed")

    print("\n==================================================")
    print("STEP 2: Testing XXRI Store Window HELD-BUTTON Move")
    print("==================================================")

    # 1. Click Store on dock (x=392, y=719)
    print("Launching Store...")
    vm.click(392, 719)
    time.sleep(9.0)
    ppm_store_opened = vm.shot("store-01-opened")

    s_controls = find_controls_in_ppm(ppm_store_opened)
    print(f"Store controls located at: {s_controls}")
    if not s_controls:
        print("ERROR: Could not locate Store window controls!")
        sys.exit(1)

    s_min_x, s_min_y = s_controls
    s_drag_start_x = s_min_x + 250
    s_drag_start_y = s_min_y + 10

    s_drag_target_x = s_drag_start_x + 180
    s_drag_target_y = s_drag_start_y + 120
    s_total_dx = s_drag_target_x - s_drag_start_x
    s_total_dy = s_drag_target_y - s_drag_start_y

    print(f"Pressing and HOLDING mouse at ({s_drag_start_x}, {s_drag_start_y})...")
    vm.move(s_drag_start_x, s_drag_start_y)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    for i, frac in enumerate([0.25, 0.50, 0.75, 1.0], 1):
        cur_x = int(s_drag_start_x + s_total_dx * frac)
        cur_y = int(s_drag_start_y + s_total_dy * frac)
        print(f"  Holding & dragging Store to step {i}/4: ({cur_x}, {cur_y})...")
        vm.move(cur_x, cur_y)
        time.sleep(0.3)
        vm.shot(f"store-02-held-frame-{i}")

    print("Releasing mouse button...")
    vm.button(False, "left")
    time.sleep(0.8)
    ppm_store_moved = vm.shot("store-03-moved-released")

    new_s_controls = find_controls_in_ppm(ppm_store_moved)
    print(f"Store controls at NEW position: {new_s_controls}")
    if not new_s_controls:
        print("ERROR: Store controls not found after move!")
        sys.exit(1)

    new_s_min_x, new_s_min_y = new_s_controls
    print(f"Store shift detected: dx={new_s_min_x - s_min_x}, dy={new_s_min_y - s_min_y}")

    # Click visible control at NEW position: "Refresh" button (dx~380, dy~20 relative to controls)
    ref_x = new_s_min_x + 380
    ref_y = new_s_min_y + 20
    print(f"Clicking 'Refresh' at NEW position ({ref_x}, {ref_y})...")
    vm.click(ref_x, ref_y)
    time.sleep(1.2)
    vm.shot("store-04-clicked-new-control")

    # Click close button at NEW position
    new_s_close_x = new_s_min_x + 30
    new_s_close_y = new_s_min_y
    print(f"Clicking Store close control at NEW position ({new_s_close_x}, {new_s_close_y})...")
    vm.click(new_s_close_x, new_s_close_y)
    time.sleep(2.0)
    vm.shot("store-05-closed")

    print("\nALL HELD MOVE TESTS COMPLETED SUCCESSFULLY!")

if __name__ == "__main__":
    main()
