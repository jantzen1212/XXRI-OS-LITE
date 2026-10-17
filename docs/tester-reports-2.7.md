# XXRI OS Lite 2.7 - tester reports

What the code shows for each report, what was changed, and what is still
needed from the tester. Checked on the host only (device busybox in bwrap,
Xvfb, fake sysfs/procfs/xrandr), not on the reporters' machines.

## Dock icons missing

`xxri-dock-pin boot` writes all 8 pinned entries (C, en_US.UTF-8,
pt_PT.UTF-8). wbar.tcz's install hook runs from setup.lst, after every
extension and `ldconfig`. No fault found on that path. With two displays
connected the dock could sit in the gap between them (see Two monitors).

Needed: a photo (no dock, empty pill or blank squares), `xrandr`,
`cat /etc/sysconfig/icons`, `pidof wbar`, `cat /usr/local/tce.icons`,
`ls -la /usr/local/share/xxri-launcher/framed`, `/tmp/xxri-dock-pin.log`.

## Quick Control: Settings and Power do nothing (fixed)

`clickable()` put the gear, power and mute glyphs, each a GtkDrawingArea,
in an event box without a visible window. The drawing area has its own
GdkWindow above the box's input window, so clicks on it never reached the
handler. The Wi-Fi and Bluetooth tiles only worked on their labels. Fixed
with `gtk_event_box_set_above_child()`. The Store's rail buttons use the
same pattern; not reported, not changed.

## Text during boot

| Text | Cause |
|---|---|
| `Welcome to GRUB!` | upstream GRUB 2.14 banner, harmless |
| `Booting in blind mode` | BIOS without any gfxpayload mode; no splash |
| kernel lines (`loglevel=3`) | emergency/alert/critical messages only |
| `Errors occured during the loading of the extensions` | real failure |
| `init: ... falling back to RAM boot`, fsck warning | real failure (installed) |

Glitches on the HP g6: the display mode changes up to three times during
boot (firmware framebuffer, KMS, 1024x768 from `.xsession`). On the AMD
variants, glamor on r600-class GPUs is another candidate. Nothing changed.

Needed: photo or video, the boot menu entry used, `dmesg -l
emerg,alert,crit,err`, `/var/log/xxri-boot.log`, `/tmp/xorg.log`,
`lspci -nn`; for the g6 also a boot with "safe graphics".

## No sound on the HP g6 (fixed in part)

`xxri-audio init` saved its automatic choice as a card number and reused it
on every boot. If the numbering changed, it pointed at the HDMI card for
good, and Settings has no control for it. The automatic choice is no longer
saved; `xxri-audio default set` saves the ALSA card id; an old saved number
is ignored. HDMI-only cards are now recognised by their PCM ids ("HDMI n")
instead of the card name.

Not fixable blind: codec quirks such as IDT speaker auto-mute. Needed:
`cat /proc/asound/cards`, `aplay -l`, `cat /etc/asound.conf`,
`/usr/local/etc/xxri/audio.conf`, `amixer -c N` per card, and whether
headphones play.

## Two monitors on the HP g6 (fixed, untested on hardware)

XLibre places a second output to the right of the first. `.xsession` set
only the first output to 1024x768, so the root window spanned both screens
with a dead strip, and the dock, Quick Control and maximised windows were
placed on that root window. Now the built-in panel is the primary at 0,0
and other displays mirror it, or are switched off if they lack its mode.
Single-display machines run the same commands as before. Settings > Display
uses the primary output.

The fix is in `/etc/skel/.xsession`: existing homes keep their old copy.

Needed: `xrandr` with the monitor attached, and a photo.

## Installer on the Dell Inspiron 3593

Likely causes:

1. Storage set to "RAID On" (Intel RST/VMD). `CONFIG_VMD` needs a 64-bit
   kernel, so the i686 kernel does not see the NVMe drive and there is
   nothing to install to. Workaround: AHCI in firmware setup.
2. 64-bit-only UEFI: the i686 release has a 32-bit UEFI loader only, so
   the machine must boot in Legacy/CSM with Secure Boot off.

Fixed while checking this: the installer listed, and pre-selected when it
was the only disk, the USB stick XXRI was running from. Its check compared
against `/`, which is RAM on a live boot. That disk is now left out of the
list and refused in auto mode.

Needed: the page it stopped on, firmware boot mode and storage setting,
`lspci -nn`, `cat /proc/partitions`, `xxri-install-backend list-disks`.

## Log out with "ask for login" gave a root shell (fixed)

`xxri-desktop restart-session` removed `/var/log/autologin`, so tty1 ran
root's `.profile` again. With `noautologin` already emptied at boot it
matched neither branch and stayed a root shell. Logout now keeps
`/var/log/autologin` when autologin is off.
