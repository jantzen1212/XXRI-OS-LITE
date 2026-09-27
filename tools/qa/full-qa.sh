#!/bin/bash
# tools/qa/full-qa.sh ISO OUTDIR [BASELINE_ISO]  - every 2.6 boot test, one ISO (QA only)
#
#   gate      validate-release.sh --boot (SeaBIOS + 32-bit OVMF x CD/disk/USB,
#             64-bit OVMF refusal, direct core.gz integrity)
#   matrix    RAM 256 MiB .. 16 GiB (BIOS) / 8 GiB (IA32 UEFI), default entry
#             and serial-console runs; media x RAM
#   rufus     Rufus ISO-mode emulation: MBR on BIOS + IA32 UEFI, GPT on IA32
#             UEFI (and GPT on BIOS, which Rufus does not offer: expected no boot)
#   ventoy    Ventoy 1.1.17, MBR and GPT (-g) sticks, on BIOS + IA32 UEFI
#   install   the five installer scenarios, each followed by a boot of the
#             installed disk alone
#   baseline  the 2.6 RC1 ISO on BIOS and IA32 UEFI, for comparison
set -u
ISO=$(readlink -f "$1") OUT=$(readlink -f "$2") BASE=${3:+$(readlink -f "$3")}
T=$(cd "$(dirname "$0")/.." && pwd)
FW=/home/jantzen/.xxri-testenv/fw
mkdir -p "$OUT"
line() { printf '%s\n' "$*" | tee -a "$OUT/SUMMARY.txt"; }
: > "$OUT/SUMMARY.txt"
line "ISO $(basename "$ISO")  sha256 $(sha256sum "$ISO" | cut -c1-16)...  $(date -u +%FT%TZ)"

line "== gate"
"$T/../validate-release.sh" "$ISO" --boot > "$OUT/gate.log" 2>&1
grep -E '^  (FAIL|warn)|^== ' "$OUT/gate.log" | tee -a "$OUT/SUMMARY.txt"
grep -E '^  ok +(bios|ia32)/|x64 UEFI|direct kernel' "$OUT/gate.log" | tee -a "$OUT/SUMMARY.txt"

line "== matrix: RAM"
python3 "$T/boot-matrix.py" "$ISO" "$OUT/matrix-ram" ram > /dev/null 2>&1
column -t -s $'\t' "$OUT/matrix-ram/summary.tsv" | cut -c1-200 | tee -a "$OUT/SUMMARY.txt"
line "== matrix: media"
python3 "$T/boot-matrix.py" "$ISO" "$OUT/matrix-media" media > /dev/null 2>&1
column -t -s $'\t' "$OUT/matrix-media/summary.tsv" | cut -c1-120 | tee -a "$OUT/SUMMARY.txt"

bootline() { # label fw img
	python3 "$T/qemu-boot-test.py" --fw "$2" --media usb --mem 2048 --img "$3" --shots 90 \
		--timeout 130 --out "$OUT/$1" 2>/dev/null | python3 -c "
import json,sys
r=json.loads(sys.stdin.read().strip().splitlines()[-1])
b=r['shots'][0]['brightness'] if r['shots'] else None
print('%-22s grub_menu=%s userspace=%s screen@90s=%s' % ('$1', r['grub_menu'], r['userspace'], b))" | tee -a "$OUT/SUMMARY.txt"
}
line "== rufus (emulated ISO mode)"
"$T/qa/rufus-emulate.sh" "$ISO" "$OUT/rufus-mbr.img" mbr "$FW/rufus/core.img-2.14" >/dev/null 2>&1
"$T/qa/rufus-emulate.sh" "$ISO" "$OUT/rufus-gpt.img" gpt >/dev/null 2>&1
bootline rufus-mbr-bios bios "$OUT/rufus-mbr.img" &
bootline rufus-mbr-ia32 ia32 "$OUT/rufus-mbr.img" &
bootline rufus-gpt-ia32 ia32 "$OUT/rufus-gpt.img" &
bootline rufus-gpt-bios-expect-no bios "$OUT/rufus-gpt.img" &
wait
line "== ventoy"
"$T/qa/ventoy-emulate.sh" "$FW/ventoy/ventoy-1.1.17" "$ISO" "$OUT/ventoy.img" mbr >/dev/null 2>&1
"$T/qa/ventoy-emulate.sh" "$FW/ventoy/ventoy-1.1.17" "$ISO" "$OUT/ventoy-gpt.img" gpt >/dev/null 2>&1
bootline ventoy-mbr-bios bios "$OUT/ventoy.img" &
bootline ventoy-mbr-ia32 ia32 "$OUT/ventoy.img" &
bootline ventoy-gpt-bios bios "$OUT/ventoy-gpt.img" &
bootline ventoy-gpt-ia32 ia32 "$OUT/ventoy-gpt.img" &
wait
rm -f "$OUT"/rufus-*.img "$OUT"/ventoy*.img

line "== install"
python3 "$T/qa/install-test.py" "$ISO" "$OUT/install" > "$OUT/install.log" 2>&1
grep -E '^(bios|ia32)-' "$OUT/install.log" | tee -a "$OUT/SUMMARY.txt"
find "$OUT/install" -name target.img -delete

if [ -n "$BASE" ]; then
	line "== baseline $(basename "$BASE")"
	bootline baseline-bios bios "$BASE" &
	bootline baseline-ia32 ia32 "$BASE" &
	wait
fi
line "== done $(date -u +%FT%TZ)"
