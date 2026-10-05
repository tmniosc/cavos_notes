#!/usr/bin/env bash
# mkimage_fs.sh — Lab 0x09: ổ 96 MiB giống disk.img của cavOS:
#   partition 1 (bootable): FAT32 1..40 MiB  -> kernel.bin, limine/, EFI/   (cavOS mount "/boot/")
#   partition 2           : ext2 40 MiB..hết -> cây thư mục ở rootfs/     (cavOS mount "/")
# Không cần sudo: mtools cho FAT32, "mke2fs -d" chép thư mục vào ext2 lúc tạo.
set -euo pipefail

KERNEL="${1:-kernel.bin}"
IMG="${2:-os.img}"
LIMINE_DIR="${LIMINE_DIR:-$HOME/opt/limine}"
CONF="${CONF:-limine.conf}"
P1_MIB=1
P2_MIB=40
SIZE_MIB=96

echo "[1/6] create ${SIZE_MIB} MiB raw image -> $IMG"
rm -f "$IMG"
dd if=/dev/zero of="$IMG" bs=1M count=$SIZE_MIB status=none

echo "[2/6] MBR: partition 1 FAT32 (boot flag) ${P1_MIB}..${P2_MIB} MiB, partition 2 Linux (0x83) ${P2_MIB} MiB..end"
parted -s "$IMG" mklabel msdos
parted -s "$IMG" mkpart primary fat32 ${P1_MIB}MiB ${P2_MIB}MiB
parted -s "$IMG" mkpart primary ext2 ${P2_MIB}MiB 100%
parted -s "$IMG" set 1 boot on

echo "[3/6] limine bios-install"
"$LIMINE_DIR/limine" bios-install "$IMG" >/dev/null

echo "[4/6] FAT32 on partition 1, same layout as cavOS: /kernel.bin, /limine/, /EFI/BOOT/"
mformat -i "$IMG@@${P1_MIB}M" -F -v LABBOOT ::
mmd -i "$IMG@@${P1_MIB}M" ::/limine ::/EFI ::/EFI/BOOT
mcopy -i "$IMG@@${P1_MIB}M" "$KERNEL"                     ::/kernel.bin
mcopy -i "$IMG@@${P1_MIB}M" "$CONF"                       ::/limine/limine.conf
mcopy -i "$IMG@@${P1_MIB}M" "$LIMINE_DIR/limine-bios.sys" ::/limine/
mcopy -i "$IMG@@${P1_MIB}M" "$LIMINE_DIR/BOOTX64.EFI"     ::/EFI/BOOT/

echo "[5/6] ext2 on partition 2 (block size 1024, files from rootfs/)"
ROOT=$(mktemp -d)
mkdir -p "$ROOT/etc" "$ROOT/bin" "$ROOT/docs/deep/nested"
printf 'Hello from the ext2 partition (cavOS mounts it at "/").\nThis line is the second line of /etc/motd.\n' > "$ROOT/etc/motd"
printf 'root:x:0:0::/root:/bin/bash\n' > "$ROOT/etc/passwd"
printf 'four directories down\n' > "$ROOT/docs/deep/nested/note.txt"
# 300 KiB: with 1 KiB blocks it needs the 12 direct blocks, the single indirect block (256 more)
# and the double indirect block for the rest
python3 -c 'import sys; sys.stdout.buffer.write(bytes((i * 7 + i // 251) & 0xFF for i in range(300 * 1024)))' > "$ROOT/big.bin"
cp "$KERNEL" "$ROOT/bin/kernel-copy.bin"
ln -s /etc/motd "$ROOT/motd-link"
OFF=$((P2_MIB * 1024 * 1024))
P2_BLOCKS=$(( (SIZE_MIB - P2_MIB) * 1024 ))
mke2fs -q -F -t ext2 -b 1024 -L labroot -E offset=$OFF -d "$ROOT" "$IMG" $P2_BLOCKS

echo "[6/6] expected FNV-1a (computed on the host)"
python3 - "$KERNEL" "$ROOT/big.bin" "$ROOT/docs/deep/nested/note.txt" <<'PY'
import sys
for p in sys.argv[1:]:
    h = 0x811c9dc5
    d = open(p, 'rb').read()
    for b in d:
        h = ((h ^ b) * 16777619) & 0xffffffff
    print(f"  {p.split('/')[-1]:14s} {len(d):8d} bytes  FNV-1a 0x{h:x}")
PY
rm -rf "$ROOT"
echo "done: $IMG"
