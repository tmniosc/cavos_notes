#!/usr/bin/env bash
# run_all.sh — Lab 0x0b: chạy 3 bản build (fxsave như cavOS, DEMO=-DXSAVE, DEMO=-DNO_FPU_SAVE) và ghi một log.
#   bash scripts/run_all.sh <log>
set -u
LOG=$(realpath -m "${1:-lab-0x0b-run.txt}")
cd "$(dirname "$0")/.."
FLAGS="-M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none -cpu max -display none"
{
echo "# Lab 0x0b - SSE & FPU: real serial output (COM1), captured $(date '+%Y-%m-%d %H:%M %z')"
echo "# $(qemu-system-x86_64 --version | head -1); $(~/opt/limine/limine --version | head -1) (BIOS); toolchain: $(make -s info 2>/dev/null | sed -n 's/toolchain : //p') -O2; TCG with -cpu max (AVX available)"
echo "# QEMU: qemu-system-x86_64 $FLAGS -drive file=os.img,format=raw,if=ide"
echo "# Boot lines from [networking] to the syscall setup are the same as Lab 0x0a and are cut here (marked ...)."
echo
for run in "1: make run (fxsave/fxrstor, like cavOS)|" "2: DEMO=-DXSAVE|-DXSAVE" "3: DEMO=-DNO_FPU_SAVE|-DNO_FPU_SAVE"; do
    title=${run%%|*}; demo=${run#*|}
    echo "########## Run $title"
    make -s clean >/dev/null; make -s DEMO="$demo" >/dev/null 2>&1; make -s image >/dev/null 2>&1
    timeout 40 qemu-system-x86_64 $FLAGS -drive file=os.img,format=raw,if=ide 2>&1 \
        | grep -v 'terminating on signal' \
        | awk 'BEGIN{p=1} /^\[networking\]/{print "..."; p=0} /FPU switch mode/{p=1} p'
    echo
done
} > "$LOG"
make -s clean >/dev/null
echo "log: $LOG ($(wc -l < "$LOG") lines)"
