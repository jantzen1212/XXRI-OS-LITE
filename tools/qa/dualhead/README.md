# Dual-head QEMU rig

A real two-output X server for the display work: QEMU's `virtio-vga,max_outputs=2` shows the
guest two DRM connectors (Virtual-1/Virtual-2), XLibre runs on them with modesetting, and the
guest takes its own screenshots of the X root window (QEMU's per-head `screendump` is skewed
when the root is wider than a head).

## One-time setup (nothing is installed; no root)

Arch's QEMU ships the virtio-gpu and D-Bus display modules as separate packages. Unpack the ones
that match `qemu-base` into a private directory (use the installed version, here 11.1.2-1):

    mkdir -p ~/.xxri-testenv/qemu-mods/dl && cd ~/.xxri-testenv/qemu-mods
    for p in qemu-hw-display-virtio-gpu qemu-hw-display-virtio-gpu-pci qemu-hw-display-virtio-vga \
             qemu-ui-dbus qemu-ui-opengl; do
        curl -fO --output-dir dl https://geo.mirror.pkgbuild.com/extra/os/x86_64/$p-11.1.2-1-x86_64.pkg.tar.zst
        tar --zstd -xf dl/$p-*.pkg.tar.zst usr/lib/qemu
    done

`run.py` sets `QEMU_MODULE_DIR` to it. The second head only exists once a display UI asks for it:
`-display dbus` on a private session bus, then `SetUIInfo` on `/org/qemu/Display1/Console_1`
(`AQD_UI` lines from the guest, or `AQD_EARLY=1` before boot).

Build the two guest tools with the x86_64 PTXdist toolchain against `platform-xxri64/sysroot-target`
(`xdump.c` = root window to PPM, `xscr.c` = what an X client sees: screen size and RandR monitors).

## Run

    python3 run.py ISO OUTDIR [--overlay SRC:DEST:MODE ...]
      AQD_HOOK=xxri-qa-dual.sh   guest script (default): live desktop, hotplug, mirror, extend, shots
      AQD_EARLY=1                second display plugged in before boot
      AQD_W1/AQD_H1              second display's size (default 1920x1080)
      --overlay xdump:opt/aqa/xdump:755 --overlay display.conf:usr/local/etc/xxri/display.conf:644
                                 (MODE=1366x768, LAYOUT=extend ... for a saved state)

Output: `aqd.log` (xrandr, layouts, window geometry), `x-<name>.png` (the real screen).
