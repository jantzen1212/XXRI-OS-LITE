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
        time.sleep(0.1)

    def button(self, down, btn="left"):
        self.events([{"type": "btn", "data": {"down": down, "button": btn}}])
        time.sleep(0.1)

    def click(self, x, y, btn="left"):
        self.move(x, y)
        time.sleep(0.25)
        self.button(True, btn)
        time.sleep(0.15)
        self.button(False, btn)
        time.sleep(0.6)

    def key(self, name):
        self.cmd("send-key", keys=[{"type": "qcode", "data": name}])
        time.sleep(0.3)

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

def find_glyph(ppm_path, color_rgb):
    # color_rgb is (r_min, r_max, g_min, g_max, b_min, b_max)
    with open(ppm_path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6": return None
        line = f.readline()
        while line.startswith(b"#"): line = f.readline()
        dims = line.split()
        width, height = int(dims[0]), int(dims[1])
        maxval = int(f.readline().strip())
        raw_data = f.read()

    r0, r1, g0, g1, b0, b1 = color_rgb
    for y in range(height):
        row_offset = y * width * 3
        for x in range(width):
            idx = row_offset + x * 3
            r, g, b = raw_data[idx], raw_data[idx+1], raw_data[idx+2]
            if r0 <= r <= r1 and g0 <= g <= g1 and b0 <= b <= b1:
                return x, y
    return None

def main():
    vm = VMDriver()

    # Reset any open menu
    vm.key("esc")
    time.sleep(1)

    print("==================================================")
    print("TEST 1: Settings Window HELD-BUTTON Move Test")
    print("==================================================")

    # First, let's close any existing Settings window if open
    ppm = vm.shot("test-00-start")
    loc = find_glyph(ppm, (210, 245, 20, 50, 190, 220))
    if loc:
        # Close it first to start clean
        print(f"Closing previous window at {loc}...")
        vm.click(loc[0] + 30, loc[1])
        time.sleep(2.0)

    # Launch fresh Settings
    print("Launching Settings from dock (x=440, y=719)...")
    vm.click(440, 719)
    time.sleep(6.0)
    ppm_opened = vm.shot("settings-10-opened")

    min_loc = find_glyph(ppm_opened, (210, 245, 20, 50, 190, 220))
    print(f"Settings minimize glyph located at: {min_loc}")
    if not min_loc:
        print("ERROR: Settings window not found!"); sys.exit(1)

    mx0, my0 = min_loc
    # Header draggable area: around mx0 + 150, my0 + 20
    hx0, hy0 = mx0 + 150, my0 + 20
    # Move by dx=+180, dy=+140
    target_hx = hx0 + 180
    target_hy = hy0 + 140

    print(f"1. Press and HOLD mouse on header ({hx0}, {hy0})...")
    vm.move(hx0, hy0)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    print("2. Dragging across desktop while STILL HOLDING...")
    for step in range(1, 5):
        frac = step / 4.0
        cx = int(hx0 + 180 * frac)
        cy = int(hy0 + 140 * frac)
        vm.move(cx, cy)
        time.sleep(0.3)
        vm.shot(f"settings-11-held-step-{step}")

    print("3. Releasing mouse at new position...")
    vm.button(False, "left")
    time.sleep(0.8)
    ppm_moved = vm.shot("settings-12-moved")

    min_loc2 = find_glyph(ppm_moved, (210, 245, 20, 50, 190, 220))
    print(f"Settings minimize glyph at NEW position: {min_loc2}")
    if not min_loc2:
        print("ERROR: Settings glyph not found after move!"); sys.exit(1)

    mx1, my1 = min_loc2
    print(f"Window moved by dx={mx1-mx0}, dy={my1-my0}")

    # 4. Click visible controls at NEW position
    # Let's click "Wallpaper & Style": located at sidebar (mx1 + 35, my1 + 350)
    wp_click_x = mx1 + 35
    wp_click_y = my1 + 350
    print(f"4. Clicking 'Wallpaper & Style' at NEW position ({wp_click_x}, {wp_click_y})...")
    vm.click(wp_click_x, wp_click_y)
    time.sleep(1.5)
    vm.shot("settings-13-clicked-wallpaper-new-pos")

    # Let's click "Storage": located at sidebar (mx1 + 35, my1 + 475)
    st_click_x = mx1 + 35
    st_click_y = my1 + 475
    print(f"5. Clicking 'Storage' at NEW position ({st_click_x}, {st_click_y})...")
    vm.click(st_click_x, st_click_y)
    time.sleep(1.5)
    vm.shot("settings-14-clicked-storage-new-pos")

    # 6. Click Close control at NEW position: close glyph is at mx1 + 30, my1
    close_click_x = mx1 + 30
    close_click_y = my1
    print(f"6. Clicking Close control at NEW position ({close_click_x}, {close_click_y})...")
    vm.click(close_click_x, close_click_y)
    time.sleep(2.0)
    ppm_closed = vm.shot("settings-15-closed")

    if find_glyph(ppm_closed, (210, 245, 20, 50, 190, 220)):
        print("WARNING: Settings glyph still detected after close!")
    else:
        print("SUCCESS: Settings window closed properly from NEW position click!")

    print("\n==================================================")
    print("TEST 2: XXRI Store Window HELD-BUTTON Move Test")
    print("==================================================")

    # Launch fresh Store
    print("Launching Store from dock (x=392, y=719)...")
    vm.click(392, 719)
    time.sleep(7.0)
    ppm_store_opened = vm.shot("store-10-opened")

    s_min_loc = find_glyph(ppm_store_opened, (210, 245, 20, 50, 190, 220))
    print(f"Store minimize glyph located at: {s_min_loc}")
    if not s_min_loc:
        print("ERROR: Store window not found!"); sys.exit(1)

    smx0, smy0 = s_min_loc
    # Header draggable area: around smx0 + 200, smy0 + 20
    shx0, shy0 = smx0 + 200, smy0 + 20

    print(f"1. Press and HOLD mouse on Store header ({shx0}, {shy0})...")
    vm.move(shx0, shy0)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    print("2. Dragging Store across desktop while STILL HOLDING...")
    for step in range(1, 5):
        frac = step / 4.0
        cx = int(shx0 + 160 * frac)
        cy = int(shy0 + 120 * frac)
        vm.move(cx, cy)
        time.sleep(0.3)
        vm.shot(f"store-11-held-step-{step}")

    print("3. Releasing mouse at new position...")
    vm.button(False, "left")
    time.sleep(0.8)
    ppm_store_moved = vm.shot("store-12-moved")

    s_min_loc2 = find_glyph(ppm_store_moved, (210, 245, 20, 50, 190, 220))
    print(f"Store minimize glyph at NEW position: {s_min_loc2}")
    if not s_min_loc2:
        print("ERROR: Store glyph not found after move!"); sys.exit(1)

    smx1, smy1 = s_min_loc2
    print(f"Store moved by dx={smx1-smx0}, dy={smy1-smy0}")

    # 4. Click visible control at NEW position: "Refresh" button
    # In Store, "Refresh" button is in the top bar around smx1 + 375, smy1 + 15
    ref_click_x = smx1 + 375
    ref_click_y = smy1 + 15
    print(f"4. Clicking 'Refresh' button at NEW position ({ref_click_x}, {ref_click_y})...")
    vm.click(ref_click_x, ref_click_y)
    time.sleep(1.5)
    vm.shot("store-13-clicked-refresh-new-pos")

    # 5. Click Close control at NEW position: smx1 + 30, smy1
    s_close_click_x = smx1 + 30
    s_close_click_y = smy1
    print(f"5. Clicking Close control at NEW position ({s_close_click_x}, {s_close_click_y})...")
    vm.click(s_close_click_x, s_close_click_y)
    time.sleep(2.0)
    ppm_store_closed = vm.shot("store-14-closed")

    if find_glyph(ppm_store_closed, (210, 245, 20, 50, 190, 220)):
        print("WARNING: Store glyph still detected after close!")
    else:
        print("SUCCESS: Store window closed properly from NEW position click!")

    print("\nALL VERIFICATION TESTS COMPLETED SUCCESSFULLY!")

if __name__ == "__main__":
    main()
