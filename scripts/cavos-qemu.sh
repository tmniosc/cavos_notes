#!/usr/bin/env bash
# Chạy cavOS trong QEMU KHÔNG dùng KVM.
# Máy này là Windows 10 -> WSL2 không có nested virtualization -> /dev/kvm mở ra ENODEV.
# Cùng flag với target `qemu` trong src/kernel/Makefile, chỉ bỏ -enable-kvm.
# Dùng: ~/cavos-qemu.sh            (cửa sổ SDL)
#       ~/cavos-qemu.sh -nogfx     (headless, chỉ serial - tiện chép log)
set -euo pipefail
cd ~/cavOS/src/kernel

DISPLAY_ARGS=(-vga vmware -display sdl)
[ "${1:-}" = "-nogfx" ] && DISPLAY_ARGS=(-display none)

exec qemu-system-x86_64 -d guest_errors -serial stdio \
    -drive file=../../disk.img,format=raw,id=disk,if=none \
    -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 \
    -m 4g -netdev user,id=mynet0 -net nic,model=e1000,netdev=mynet0 \
    "${DISPLAY_ARGS[@]}"
