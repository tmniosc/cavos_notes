#!/usr/bin/env bash
# run_all.sh — Lab 0x0a: chạy 3 bản build (thường, DEMO=-DINT80_STI, DEMO=-DBAD_SYSRET) và ghi một log.
#   bash scripts/run_all.sh <log>
set -u
LOG=$(realpath -m "${1:-lab-0x0a-run.txt}")
cd "$(dirname "$0")/.."
FLAGS="-M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none -display none"
{
echo "# Lab 0x0a - Fast Syscalls: real serial output (COM1), captured $(date '+%Y-%m-%d %H:%M %z')"
echo "# $(qemu-system-x86_64 --version | head -1); $(~/opt/limine/limine --version | head -1) (BIOS); toolchain: $(make -s info 2>/dev/null | sed -n 's/toolchain : //p') -O2; TCG (no KVM on this machine)"
echo "# QEMU: qemu-system-x86_64 $FLAGS -drive file=os.img,format=raw,if=ide"
echo "# Boot lines from [networking] to the two fsMount calls are the same as Lab 0x09 and are cut here (marked ...)."
echo
for run in "1: make run|" "2: DEMO=-DINT80_STI (sti in the int 0x80 path, like cavOS)|-DINT80_STI" "3: DEMO=-DBAD_SYSRET (non-canonical RCX before sysret)|-DBAD_SYSRET"; do
    title=${run%%|*}; demo=${run#*|}
    echo "########## Run $title"
    make -s clean >/dev/null; make -s DEMO="$demo" >/dev/null 2>&1; make -s image >/dev/null 2>&1
    timeout 30 qemu-system-x86_64 $FLAGS -drive file=os.img,format=raw,if=ide 2>&1 \
        | grep -v 'terminating on signal' \
        | awk 'BEGIN{p=1} /^\[networking\]/{print "..."; p=0} /MSRs before/{p=1} p'
    echo
done
} > "$LOG"
make -s clean >/dev/null
echo "log: $LOG ($(wc -l < "$LOG") lines)"
