#!/usr/bin/env bash
# run_all.sh — Lab 0x0c: build, xem /bin/hello bằng readelf phía host, chạy QEMU, ghi một log.
#   bash scripts/run_all.sh <log>
set -u
LOG=$(realpath -m "${1:-lab-0x0c-run.txt}")
cd "$(dirname "$0")/.."
FLAGS="-M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none -cpu max -display none"
RE=$(make -s info >/dev/null 2>&1; ls ~/opt/cross/bin/x86_64-cavos-readelf 2>/dev/null || echo readelf)
{
echo "# Lab 0x0c - Userspace & ELF Loader: real output, captured $(date '+%Y-%m-%d %H:%M %z')"
echo "# $(qemu-system-x86_64 --version | head -1); $(~/opt/limine/limine --version | head -1) (BIOS); toolchain: $(make -s info 2>/dev/null | sed -n 's/toolchain : //p') -O2; TCG -cpu max"
echo "# QEMU: qemu-system-x86_64 $FLAGS -drive file=os.img,format=raw,if=ide"
echo "# Boot lines from [networking] to the syscall/SSE setup are the same as Lab 0x0b and are cut here (marked ...)."
echo
make -s clean >/dev/null; make -s >/dev/null 2>&1
echo "##### host: make image"
make -s image 2>&1 | grep -E '^\[[0-9]/6\]|^  /bin|^done'
echo
echo "##### host: $(basename "$RE") -hlSW userbin/hello (trimmed)"
"$RE" -hlSW userbin/hello | grep -E 'Type:|Machine:|Entry point|Start of program|Size of program|Number of program|\] \.(text|rodata|data|bss)|LOAD|^ +0[01] '
echo
echo "##### QEMU serial"
timeout 40 qemu-system-x86_64 $FLAGS -drive file=os.img,format=raw,if=ide 2>&1 \
    | grep -v 'terminating on signal' \
    | awk 'BEGIN{p=1} /^\[networking\]/{print "..."; p=0} /syscalls and SSE ready/{p=1} p'
} > "$LOG"
make -s clean >/dev/null
echo "log: $LOG ($(wc -l < "$LOG") lines)"
