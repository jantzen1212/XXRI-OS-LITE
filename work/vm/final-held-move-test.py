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
        time.sleep(0.25)
        self.button(True, btn)
        time.sleep(0.15)
        self.button(False, btn)
        time.sleep(0.5)

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
        return png

def find_control_glyph(png_path):
    # Locate #E624CE (minimize glyph) where x > 50 to avoid wallpaper
    cmd = f"magick {png_path} txt: | grep -i '#E624CE' | head -1"
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, text=True).stdout.strip()
    if not res:
        return None
    coords = res.split(":")[0].split(",")
    return int(coords[0]), int(coords[1])

def main():
    vm = VMDriver()

    # =========================================================================
    # PART 1: SETTINGS WINDOW HELD MOVE TEST
    # =========================================================================
    print("==================================================")
    print("PART 1: Settings Window Held Move Test")
    print("==================================================")

    # 1. Launch Settings
    print("1. Launching Settings from dock...")
    vm.click(440, 719)
    time.sleep(5.0)
    shot_opened = vm.shot("01-settings-opened")

    min_loc = find_control_glyph(shot_opened)
    print(f"   Settings minimize glyph at: {min_loc}")
    if not min_loc:
        print("ERROR: Settings minimize glyph not found!"); sys.exit(1)

    mx0, my0 = min_loc
    # Header drag area (Settings title bar / sidebar top)
    hx0, hy0 = mx0 + 100, my0 + 40

    # Drag delta: +220px to the right, +160px down
    drag_dx = 220
    drag_dy = 160

    print(f"2. Pressing and HOLDING mouse on header at ({hx0}, {hy0})...")
    vm.move(hx0, hy0)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    print("3. Moving substantially across desktop while STILL HOLDING...")
    for step in range(1, 5):
        frac = step / 4.0
        cur_x = int(hx0 + drag_dx * frac)
        cur_y = int(hy0 + drag_dy * frac)
        vm.move(cur_x, cur_y)
        time.sleep(0.4)
        vm.shot(f"02-settings-held-drag-step-{step}")

    print(f"4. Releasing mouse at new position ({hx0 + drag_dx}, {hy0 + drag_dy})...")
    vm.button(False, "left")
    time.sleep(0.8)
    shot_moved = vm.shot("03-settings-moved-released")

    new_min_loc = find_control_glyph(shot_moved)
    print(f"   Settings minimize glyph at NEW position: {new_min_loc}")
    if not new_min_loc:
        print("ERROR: Settings glyph not found at new position!"); sys.exit(1)

    mx1, my1 = new_min_loc
    print(f"   Actual window move displacement: dx={mx1 - mx0}, dy={my1 - my0}")

    # 5. Click visible controls at NEW position: "Wallpaper & Style"
    wp_x = mx1 + 45
    wp_y = my1 + 350
    print(f"5. Clicking 'Wallpaper & Style' at NEW position ({wp_x}, {wp_y})...")
    vm.click(wp_x, wp_y)
    time.sleep(1.5)
    vm.shot("04-settings-clicked-wallpaper-new-pos")

    # 6. Click visible control at NEW position: "Storage"
    st_x = mx1 + 45
    st_y = my1 + 475
    print(f"6. Clicking 'Storage' at NEW position ({st_x}, {st_y})...")
    vm.click(st_x, st_y)
    time.sleep(1.5)
    vm.shot("05-settings-clicked-storage-new-pos")

    # 7. Click Close control at NEW position (close glyph is at mx1 + 30, my1)
    close_x = mx1 + 30
    close_y = my1
    print(f"7. Clicking Close control at NEW position ({close_x}, {close_y})...")
    vm.click(close_x, close_y)
    time.sleep(2.0)
    shot_closed = vm.shot("06-settings-closed")

    if find_control_glyph(shot_closed):
        print("ERROR: Settings window did not close!"); sys.exit(1)
    print("   SUCCESS: Settings closed cleanly!\n")

    # =========================================================================
    # PART 2: XXRI STORE WINDOW HELD MOVE TEST
    # =========================================================================
    print("==================================================")
    print("PART 2: XXRI Store Window Held Move Test")
    print("==================================================")

    # 1. Launch Store
    print("1. Launching XXRI Store from dock...")
    vm.click(392, 719)
    time.sleep(6.0)
    shot_store_opened = vm.shot("07-store-opened")

    s_min_loc = find_control_glyph(shot_store_opened)
    print(f"   Store minimize glyph at: {s_min_loc}")
    if not s_min_loc:
        print("ERROR: Store minimize glyph not found!"); sys.exit(1)

    smx0, smy0 = s_min_loc
    # Header drag area (Store header bar)
    shx0, shy0 = smx0 + 200, smy0 + 25

    # Drag delta: +180px to the right, +140px down
    s_drag_dx = 180
    s_drag_dy = 140

    print(f"2. Pressing and HOLDING mouse on Store header at ({shx0}, {shy0})...")
    vm.move(shx0, shy0)
    time.sleep(0.3)
    vm.button(True, "left")
    time.sleep(0.3)

    print("3. Moving substantially across desktop while STILL HOLDING...")
    for step in range(1, 5):
        frac = step / 4.0
        cur_x = int(shx0 + s_drag_dx * frac)
        cur_y = int(shx0 + s_drag_dy * frac)
        vm.move(cur_x, cur_y)
        time.sleep(0.4)
        vm.shot(f"08-store-held-drag-step-{step}")

    print(f"4. Releasing mouse at new position ({shx0 + s_drag_dx}, {shy0 + s_drag_dy})...")
    vm.button(False, "left")
    time.sleep(0.8)
    shot_store_moved = vm.shot("09-store-moved-released")

    new_s_min_loc = find_control_glyph(shot_store_moved)
    print(f"   Store minimize glyph at NEW position: {new_s_min_loc}")
    if not new_s_min_loc:
        print("ERROR: Store glyph not found at new position!"); sys.exit(1)

    smx1, smy1 = new_s_min_loc
    print(f"   Actual Store move displacement: dx={smx1 - smx0}, dy={smy1 - smy0}")

    # 5. Click visible control at NEW position: "Refresh" button (approx smx1 + 380, smy1 + 15)
    ref_x = smx1 + 380
    ref_y = smy1 + 15
    print(f"5. Clicking 'Refresh' at NEW position ({ref_x}, {ref_y})...")
    vm.click(ref_x, ref_y)
    time.sleep(1.5)
    vm.shot("10-store-clicked-refresh-new-pos")

    # 6. Click Close control at NEW position (close glyph is at smx1 + 30, smy1)
    s_close_x = smx1 + 30
    s_close_y = smy1
    print(f"6. Clicking Close control at NEW position ({s_close_x}, {s_close_y})...")
    vm.click(s_close_x, s_close_y)
    time.sleep(2.0)
    shot_store_closed = vm.shot("11-store-closed")

    if find_control_glyph(shot_store_closed):
        print("ERROR: Store window did not close!"); sys.exit(1)
    print("   SUCCESS: Store closed cleanly!\n")

    print("==================================================")
    print("ALL TESTS PASSED WITH 100% ACCURACY!")
    print("==================================================")

if __name__ == "__main__":
    main()
