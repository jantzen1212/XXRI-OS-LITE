#!/bin/bash
# tools/qa/release-qa.sh ISO OUTDIR [REFERENCE_ISO] - the complete QA run for a
# release candidate (QA only; nothing here is shipped).
#
#   1. full-qa.sh     release gate, firmware x media boots, RAM 256 MiB..16 GiB,
#                     Rufus and Ventoy sticks, the five installer scenarios
#   2. x-test.py      display, input and acceleration/fallback paths under KVM:
#                     Xorg+modesetting (BIOS, IA32 UEFI), Xfbdev (nomodeset,
#                     BIOS and UEFI), Xorg+vesa (no display device), Cirrus,
#                     VMware SVGA (KMS driver fails -> vesa), Rage 128 (no KMS
#                     driver -> Xfbdev), and every XXRI program on the KMS path
#   3. x-test.py      the CPU classes under TCG with the ISA sentinel: Pentium
#                     (i586), Pentium II, Pentium III, an SSE2 model - whole
#                     desktop + every program; four VMs in parallel, slow
#   4. REFERENCE_ISO  (the 2.6 release) through the same app check, for the
#                     memory comparison
#
# Not in the matrix: gpu-rv100 - radeon KMS hangs QEMU's ati-vga emulation
# (see docs/x-server.md), which says nothing about a real Radeon.
#
# XXRI_SENTINEL_DIR: directory holding isa-sentinel.so, the TCG plugin built
# from tools/qa/isa-sentinel.c (gcc -shared -fPIC with QEMU's qemu-plugin.h).
set -u
ISO=$(readlink -f "$1") OUT=$(readlink -f "$2") REF=${3:+$(readlink -f "$3")}
T=$(cd "$(dirname "$0")" && pwd)
export XXRI_SENTINEL_DIR=${XXRI_SENTINEL_DIR:-$T}
mkdir -p "$OUT"
log() { printf '%s %s\n' "$(date -u +%FT%TZ)" "$*" | tee -a "$OUT/release-qa.log"; }

log "ISO $ISO sha256 $(sha256sum "$ISO" | cut -d' ' -f1)"
log "1/4 boot matrix"
"$T/full-qa.sh" "$ISO" "$OUT/matrix" > "$OUT/matrix.log" 2>&1
log "2/4 display, input, acceleration/fallback, apps (KVM)"
python3 "$T/x-test.py" "$ISO" "$OUT/x" kms-bios kms-ia32 fbdev-bios fbdev-ia32 vesa-bios \
	gpu-cirrus gpu-vmware gpu-rage128 apps-kvm > "$OUT/x.log" 2>&1
log "3/4 CPU classes (TCG + ISA sentinel)"
for s in cpu-pentium cpu-pentium2 cpu-pentium3 cpu-sse2; do
	python3 "$T/x-test.py" "$ISO" "$OUT/$s" "$s" > "$OUT/$s.log" 2>&1 &
done
wait
if [ -n "$REF" ]; then
	log "4/4 reference ISO app check"
	python3 "$T/x-test.py" "$REF" "$OUT/reference" apps-kvm > "$OUT/reference.log" 2>&1
fi
log "done"
cat "$OUT/matrix/SUMMARY.txt" "$OUT/x.log" "$OUT"/cpu-*.log ${REF:+"$OUT/reference.log"} 2>/dev/null
