XXRI OS Lite 2.5 - NEC VersaPro VK24MX: core.gz handoff test
=============================================================

Three test images (check them against SHA256SUMS):
  A  XXRI-VK24MX-diag-A-isolinux405.iso   same boot loader as the release  (RUN FIRST)
  B  XXRI-VK24MX-diag-B-isolinux6.iso     newer ISOLINUX 6.04
  C  XXRI-VK24MX-diag-C-grub.iso          GRUB 2 (completely different loader)
All three contain the unchanged XXRI 2.5 system. Entries 1-3 run a small
checker BEFORE XXRI starts: it compares every byte of core.gz that the kernel
actually received with the real file, then either continues the normal boot
(if everything matches) or stops and shows what went wrong.

Before you start, write down
----------------------------
  - Laptop label (full model number, e.g. PC-VK24MXZ?U)
  - The program used to write the stick that failed originally
  - USB stick brand and size; which port (blue USB 3 or black USB 2)

1. Write image A to the stick
-----------------------------
  balenaEtcher (it verifies automatically), or Rufus -> "DD Image mode",
  or Linux: dd if=XXRI-VK24MX-diag-A-isolinux405.iso of=/dev/sdX bs=4M conv=fsync
  If you can, verify the stick afterwards on the same PC (Linux):
      head -c $(stat -c%s XXRI-VK24MX-diag-A-isolinux405.iso) /dev/sdX | sha256sum
  and compare with SHA256SUMS.

2. BIOS (F2 at the NEC logo)
----------------------------
  Main      -> photograph (we need "UEFI BIOS Version")
  Security  -> Secure Boot -> Secure Boot ........ Disabled
  Startup   -> UEFI/Legacy Boot .................. Legacy Only
  Config    -> USB -> USB UEFI BIOS Support ...... Enabled
  Restart   -> Exit Saving Changes

3. Boot image A (F12 at the NEC logo -> "USB HDD")
--------------------------------------------------
  Run these entries, rebooting in between:

  Entry 1  "Check core.gz handoff (standard kernel)"
           A report appears. Photograph the WHOLE screen.
           - "HANDOFF OK": let it continue and tell us whether the desktop appears.
           - Anything else: the screen stays; photograph it. At the "#" prompt
             you may type:  more /xd/report.txt   (to see anything that scrolled)
           - If you get ONLY a kernel panic and no report, photograph that too -
             that result is important.
  Entry 2  Same check, core.gz placed low in memory. Photograph the report.
  Entry 5  Normal XXRI boot. Desktop or not? Photograph the last screen.
  (Entry 3 = entry 1 in text mode, only if entry 1 shows a black screen.
   Entry 4 = verbose boot without the checker, only if we ask for it.)

4. Only if image A entry 1 or entry 5 did NOT work
--------------------------------------------------
  Write image B to the stick, boot it, run entries 1 and 2, photograph both.
  Write image C to the stick, boot it, run entries 1 and 2, photograph both.
  If you have a second USB stick (different brand), repeat image A entry 1 on it.

Send back: all photos, BIOS version, the original stick-writing tool,
stick brand/size, and which port was used.
