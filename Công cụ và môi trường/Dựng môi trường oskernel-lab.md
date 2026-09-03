---
tags: [meta, build, setup, lab]
status: đang làm
---

# Dựng môi trường oskernel-lab (máy trắng)

> _Công thức dựng **riêng lab tự viết** (`~/oskernel-lab`): Limine, chép source từ vault, build, boot QEMU.
> Nhẹ hơn cavOS nhiều — **không cần** cross-compiler._

Nền chung (WSL, quy tắc build ở đâu, gói dùng chung): [[Dựng môi trường chung]] — làm trước note này.
Liên quan: [[Dựng môi trường cavOS]] · [[Cẩm nang build & debug]]

## Nói thật đơn giản

> _cavOS là **sách giáo khoa** đi mượn; lab là **vở bài tập** của mình. Mượn sách thì mất cũng mua lại được,
> còn vở bài tập mất là mất luôn — nên vở phải tự cất giữ._

Khác biệt lớn nhất so với cavOS:

- **Nguồn code**: cavOS `git clone` từ trên mạng; lab thì **chép từ vault** (`Thực hành/src/oskernel-lab/`),
  vì đây là code tự viết, không ai giữ hộ. Sửa xong nhớ **chép ngược về vault** rồi push.
- **Không cần đúc cross-compiler**: kernel lab nhỏ, gcc hệ thống với mấy cờ "freestanding" là đủ. Đỡ hàng
  chục phút chờ. (Có cross rồi thì `common.mk` tự dùng, khỏi sửa gì.)
- **Cần một bootloader rời**: Limine bản binary tải sẵn về `~/opt/limine`, dùng để cài vào ảnh đĩa `os.img`.

## 0. Máy hiện tại

| Thứ | Giá trị |
| --- | --- |
| Source lab | `~/oskernel-lab` |
| Bản lưu trong vault | `D:\tmniosc\cavos_notes\Thực hành\src\oskernel-lab` |
| Limine | `~/opt/limine` — branch `v8.x-binary` (đang dùng **8.7.0**) |
| Toolchain | gcc hệ thống; `common.mk` tự dùng cross nếu có |

## 1. Gói riêng của lab

> _Ngoài gói chung ở [[Dựng môi trường chung]], lab cần thêm đúng một thứ đáng nói: `mtools`._

```bash
sudo apt install -y mtools xorriso
```

| Gói | Để làm gì |
| --- | --- |
| `mtools` | chép file vào FAT32 **không cần mount, không cần sudo** — `mformat/mmd/mcopy`. Rất hợp WSL2 vì mount loop device trong WSL hay trục trặc |
| `xorriso` | chỉ cần nếu sau này muốn xuất ISO thay vì ảnh đĩa |

## 2. Limine (bootloader)

```bash
git clone -b v8.x-binary --depth 1 https://github.com/limine-bootloader/limine ~/opt/limine
make -C ~/opt/limine CC=gcc
~/opt/limine/limine version        # kiểm: Limine 8.7.0
```

Branch `*-binary` đã có sẵn file nhị phân (`limine-bios.sys`, `BOOTX64.EFI`...); `make` chỉ build đúng
**công cụ dòng lệnh `limine`** dùng để ghi giai đoạn BIOS vào MBR.

> **Phải là `v8.x-binary`, KHÔNG v9+.** Từ Limine 9, `kernel_path` trong `limine.conf` đổi tên và
> `LIMINE_KERNEL_ADDRESS_REQUEST` bị thay bằng `EXECUTABLE_ADDRESS` → lệch `limine.h` mà lab đang dùng
> (lấy từ cavOS) và lệch mọi note đã viết — tất cả đang ở **base revision 2**.

> `make -C ~/opt/limine` mà báo `cc: No such file or directory` thì thêm `CC=gcc`: Makefile của Limine gọi
> `cc`, mà Ubuntu sạch chưa chắc có symlink đó.

## 3. Lấy source lab về

```bash
cp -r /mnt/d/tmniosc/cavos_notes/Thực hành/src/oskernel-lab ~/oskernel-lab
chmod +x ~/oskernel-lab/scripts/mkimage.sh
```

Chép **1 chiều** vault → WSL, và **build trong `~`** (lý do ở [[Dựng môi trường chung]]).

```
~/oskernel-lab/
├─ common.mk           build chung: tự dò cross x86_64-cavos-gcc, không có thì lùi về gcc hệ thống
├─ linker.ld           bản gốc (mỗi project giữ 1 copy)
├─ limine.conf         timeout 0 · protocol limine · kernel_path boot():/boot/kernel.bin
├─ limine.h            lấy từ cavOS src/kernel/include/limine.h (base revision 2)
├─ .clangd             gỡ flag GCC-only cho clangd
├─ scripts/mkimage.sh  dựng os.img: MBR + FAT32@1MiB + limine bios-install
├─ 00-hello-serial/       Step 00 + 01
├─ 01-bootloader-parser/  Step 02
├─ 02-framebuffer/        Step 03
└─ 03-pmm-vmm/            Step 04 + 05
```

## 4. Build + chạy

```bash
cd ~/oskernel-lab/00-hello-serial
make info      # in ra đang dùng cross gcc hay gcc hệ thống
make           # kernel.bin + kernel.map/.dis/.sym/.elf.txt
make image     # os.img
make run       # QEMU headless, -serial stdio
make run-gfx   # có cửa sổ (dùng cho Lab 0x02)
```

Biến chỉnh được từ dòng lệnh, **không cần sửa file**:

```bash
make run LIMINE_DIR=/duong/dan/khac
make run QEMU_FLAGS="-M q35 -m 512M -serial stdio"
make CROSS_PREFIX=$HOME/opt/cross/bin/x86_64-cavos-
```

Chi tiết 4 artifact phân tích build và cấu trúc `os.img`: [[Cẩm nang build & debug]].

## 5. Sửa code thì nhớ chép ngược

> _Chỉ vault mới được đẩy lên GitHub. Code nằm trong WSL mà không chép ngược thì không có bản sao nào._

```bash
cp -r ~/oskernel-lab/. /mnt/d/tmniosc/cavos_notes/Thực hành/src/oskernel-lab/
cd /mnt/d/tmniosc/cavos_notes && git add "Thực hành/src" && git commit -m "lab: ..." && git push
```

Đừng chép ngược file rác build (`.o`, `kernel.bin`, `os.img`, `.map/.dis/.sym/.elf.txt`) —
`make clean` trước cho gọn.

## 6. Bẫy đã dính (đừng dính lại)

| Triệu chứng | Nguyên nhân | Cách tránh |
| --- | --- | --- |
| `make: cc: No such file or directory` khi build Limine | Makefile Limine gọi `cc` | `make -C ~/opt/limine CC=gcc` |
| `'NULL' undeclared` | freestanding không tự có `NULL`; `limine.h` chỉ kéo `stdint.h` | thêm `#include <stddef.h>` |
| `warning: .note.gnu.build-id section discarded` | `linker.ld` có `/DISCARD/ *(.note.*)` | link thêm `-Wl,--build-id=none` (đã có trong `common.mk`) |
| `.rodata` nhảy lên `…80200000` thay vì `…80001000` | thiếu `-z max-page-size=0x1000` → MAXPAGESIZE = 2 MiB | giữ cờ `-Wl,-z,max-page-size=0x1000` |
| Lab thiếu response / lỗi `kernel_path` | dùng Limine v9+ | đổi về `v8.x-binary` |
| Tên file dính khoảng trắng, build hỏng | comment cùng dòng với `VAR := value` trong Makefile | để comment ở dòng riêng |
| `make image` in "Reminder: copy limine-bios.sys" | Limine in cứng mỗi lần | **không phải lỗi** — `mkimage.sh` đã chép ở bước 5/5 |
