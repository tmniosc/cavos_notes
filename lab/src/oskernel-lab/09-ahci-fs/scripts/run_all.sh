#!/usr/bin/env bash
# run_all.sh — Lab 0x09: chạy 2 lần (bản thường, DEMO=-DPRDT_CAVOS) và ghi một log.
#   bash scripts/run_all.sh <log>
set -u
LOG=$(realpath -m "${1:-lab-0x09-run.txt}")
cd "$(dirname "$0")/.."
FLAGS="-M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none -display none"
{
echo "# Lab 0x09 - AHCI & Filesystems: real serial output (COM1), captured $(date '+%Y-%m-%d %H:%M %z')"
echo "# $(qemu-system-x86_64 --version | head -1); $(~/opt/limine/limine --version | head -1) (BIOS); toolchain: $(make -s info 2>/dev/null | sed -n 's/toolchain : //p') -O2"
echo "# QEMU: qemu-system-x86_64 $FLAGS -drive file=os.img,format=raw,if=ide  (on q35, if=ide = the ICH9 AHCI controller 00:1f.2)"
echo "# Disk: scripts/mkimage_fs.sh -> MBR, partition 1 FAT32 (Limine + kernel.bin, like cavOS /boot/), partition 2 ext2 1 KiB blocks (like cavOS /)"
echo
for run in "1 (TCG): make run||" "2: DEMO=-DPRDT_CAVOS (one PRDT entry for the whole buffer, like cavOS)|-DPRDT_CAVOS|"; do
    title=${run%%|*}; rest=${run#*|}; demo=${rest%%|*}; extra=${rest#*|}
    echo "########## Run $title"
    make -s clean >/dev/null; make -s DEMO="$demo" >/dev/null 2>&1
    if [ "$title" = "1 (TCG): make run" ]; then
        echo "##### make image (host side):"
        make -s image 2>&1 | grep -E '^\[[0-9]/6\]|^  [a-z]|^done'
    else
        make -s image >/dev/null 2>&1
    fi
    timeout 25 qemu-system-x86_64 $FLAGS $extra -drive file=os.img,format=raw,if=ide 2>&1 | grep -v 'terminating on signal'
    echo
done
} > "$LOG"
make -s clean >/dev/null
echo "log: $LOG ($(wc -l < "$LOG") lines)"
