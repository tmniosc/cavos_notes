# oskernel-lab — source dựng lại (2026-09-03)

Lab kernel x86_64 học theo vault [[Home]] / các note `Labs/Lab 0x0*`.
Bản gốc trong WSL đã mất; cây source này **dựng lại từ note**, chưa build lại lần nào
tại thời điểm ghi dòng này → xem "Trạng thái" bên dưới.

```
oskernel-lab/
├─ common.mk          build chung (tự chọn cross x86_64-cavos-gcc, không có thì dùng gcc hệ thống)
├─ linker.ld          bản gốc; mỗi project có 1 bản copy
├─ limine.conf        timeout 0, protocol limine, kernel_path boot():/boot/kernel.bin
├─ limine.h           lấy từ cavOS (src/kernel/include/limine.h) — base revision 2
├─ .clangd            gỡ flag GCC-only cho clangd
├─ scripts/mkimage.sh dựng os.img: MBR + FAT32@1MiB + limine bios-install (mtools, không cần sudo)
├─ 00-hello-serial/   Lab 0x00 — Step 00 + 01
├─ 01-bootloader-parser/ Lab 0x01 — Step 02
├─ 02-framebuffer/    Lab 0x02 — Step 03
└─ 03-pmm-vmm/        Lab 0x03 — Step 04 + 05
```

## Chạy
```bash
cd ~/oskernel-lab/03-pmm-vmm
make            # kernel.bin + kernel.map/.dis/.sym/.elf.txt
make image      # os.img
make run        # QEMU headless, -serial stdio
make run-gfx    # có cửa sổ (Lab 0x02)
```
Cần: `qemu-system-x86_64`, `mtools`, `parted`, và Limine binary ở `~/opt/limine`
(`git clone -b v8.x-binary --depth 1 https://github.com/limine-bootloader/limine ~/opt/limine && make -C ~/opt/limine`).

## Khác biệt đã biết so với lần chạy cũ ghi trong note
- `pmm_init` đánh dấu **toàn bộ** frame của bitmap (làm tròn LÊN). Lần chạy cũ (note Lab 0x03)
  cho `alloc #1 = 0x62000` và `free = 65196`, tức bản cũ chỉ đánh dấu 2 frame và để hở frame đuôi
  (`0x62000..0x6206d` vẫn chồng lên bitmap). Bản này vá chỗ đó — đã xác nhận bằng lần chạy thật.
- Font 8x8 của Lab 0x02 phủ đủ A-Z + 0-9 (bản cũ chỉ vài ký tự).

## Trạng thái (2026-09-03)
- [x] build thật — gcc hệ thống, Ubuntu 26.04, không warning
- [x] boot QEMU thật — cả 4 lab chạy đúng
- [x] chép output thật vào note (`Labs/outputs/lab-0x03-run-2026-09-03.txt`)
- [ ] chạy lại bằng cross `x86_64-cavos-gcc` sau khi `make tools` xong
