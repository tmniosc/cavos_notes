#!/usr/bin/env bash
# mkimage.sh — dựng os.img bootable: MBR + FAT32 @1MiB + Limine (BIOS & UEFI).
# Dùng mtools nên KHÔNG cần sudo/mount (hợp WSL2). Xem [[Build & Debug Cheatsheet]].
set -euo pipefail

KERNEL="${1:-kernel.bin}"
IMG="${2:-os.img}"
LIMINE_DIR="${LIMINE_DIR:-$HOME/opt/limine}"
CONF="${CONF:-limine.conf}"

for f in "$KERNEL" "$CONF" "$LIMINE_DIR/limine" "$LIMINE_DIR/limine-bios.sys" "$LIMINE_DIR/BOOTX64.EFI"; do
    [ -e "$f" ] || { echo "mkimage: missing $f" >&2; exit 1; }
done

echo "[1/5] create 64 MiB raw image -> $IMG"
rm -f "$IMG"
dd if=/dev/zero of="$IMG" bs=1M count=64 status=none

echo "[2/5] MBR + partition 1 (FAT32, boot flag) starting at 1 MiB"
parted -s "$IMG" mklabel msdos
parted -s "$IMG" mkpart primary fat32 1MiB 100%
parted -s "$IMG" set 1 boot on

echo "[3/5] limine bios-install (ghi MBR + stage vào 1 MiB trống đầu ổ)"
"$LIMINE_DIR/limine" bios-install "$IMG"

echo "[4/5] mformat FAT32 + tạo cây thư mục"
mformat -i "$IMG@@1M" -F ::
mmd -i "$IMG@@1M" ::/boot ::/boot/limine ::/EFI ::/EFI/BOOT

echo "[5/5] copy kernel + limine files vào FAT32"
mcopy -i "$IMG@@1M" "$KERNEL"                     ::/boot/kernel.bin
mcopy -i "$IMG@@1M" "$CONF"                       ::/boot/limine/limine.conf
mcopy -i "$IMG@@1M" "$LIMINE_DIR/limine-bios.sys" ::/boot/limine/
mcopy -i "$IMG@@1M" "$LIMINE_DIR/BOOTX64.EFI"     ::/EFI/BOOT/
[ -e "$LIMINE_DIR/BOOTIA32.EFI" ] && mcopy -i "$IMG@@1M" "$LIMINE_DIR/BOOTIA32.EFI" ::/EFI/BOOT/

echo "done: $IMG"
