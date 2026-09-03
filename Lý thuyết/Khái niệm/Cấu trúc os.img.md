---
tags: [khainiem, boot, disk]
---

# Cấu trúc os.img

> _`os.img` **không phải** kernel. Nó là **ảnh nguyên một ổ đĩa** — QEMU cắm vào như cắm HDD thật.
> Kernel chỉ là một file nằm bên trong nó._

Liên quan: [[Step 00 - Boot & Limine]] · [[Limine Protocol]] · [[Lab 0x00 - Hello Serial]] ·
lệnh build: [[Cẩm nang build & debug]]

## Nói thật đơn giản

> _Tưởng tượng bạn viết xong một lá thư (`kernel.bin`) và muốn người ta đọc được. Không thể đưa lá thư trần
> cho bưu điện — phải bỏ vào **phong bì có địa chỉ đúng chuẩn** thì họ mới chuyển._

- **Lá thư** = `kernel.bin`, thứ bạn thực sự viết ra.
- **Phong bì** = `os.img`: một ổ đĩa giả, bên trong có hệ thống file, có chỗ ghi "khởi động từ đâu".
- **Người đưa thư** = Limine (bootloader): máy vừa bật thì chưa ai biết `kernel.bin` là gì, cần một
  chương trình nhỏ biết cách đọc đĩa, tìm file, nạp vào RAM rồi nhảy vào.
- **Con tem ở góc phong bì** = 2 byte `0x55AA` cuối sector 0. Thiếu nó thì BIOS coi như đĩa không boot được,
  bỏ qua luôn.

Vì máy có **hai kiểu firmware** (BIOS đời cũ và UEFI đời mới), phong bì phải dán **hai địa chỉ**: một chỗ
cho BIOS đọc, một chỗ cho UEFI đọc. Cả hai rốt cuộc dẫn về cùng một lá thư.

## Bản đồ bên trong

> _Ba vùng, đọc từ đầu đĩa xuống. Mỗi vùng có đúng một nhiệm vụ._

![[osimg-layout.svg|1083]]

- **Phân vùng bắt đầu @1 MiB (sector 2048)** chứ không phải ngay sau MBR: khoảng trống đó **cố ý chừa cho
  Limine BIOS stage 2**. `limine bios-install` ghi MBR + nhét stage 2 vào đây. Vì phân vùng lệch 1 MiB nên
  mtools phải được chỉ đúng chỗ: `os.img@@1M`.
- **Hai đường boot cùng tồn tại**: BIOS đi theo MBR → `limine-bios.sys`; UEFI thì firmware tự đi tìm
  `/EFI/BOOT/BOOTX64.EFI` theo quy ước. Hai lối vào khác nhau, nhưng đều dừng ở `limine.conf` → nạp
  `kernel.bin` → nhảy `kmain`.
- **`limine.conf` (lab)**: `timeout: 0` (boot thẳng, không hiện menu),
  `kernel_path: boot():/boot/kernel.bin`, `protocol: limine` → nối vào
  [[Step 00 - Boot & Limine]] §0.1.1 (Limine ELF loader đọc `kernel.bin`).

## Mạch từ source tới lúc chạy

> _Đường đi đầy đủ: file `.c` bạn gõ ra rốt cuộc thành cái gì, và ai nạp nó._

![[lab-build-pipeline.svg|1083]]

Điểm dễ nhầm: **sản phẩm cuối không phải `kernel.bin`** mà là `os.img`. `make` chỉ dựng tới `kernel.bin`;
phải `make image` mới có ảnh đĩa, và `make run` boot chính ảnh đĩa đó. Sửa code mà chỉ chạy `make` rồi
`make run` thì QEMU vẫn boot ảnh cũ — luôn để `make run` tự lo cả hai bước.

## Soi nhanh os.img

> _Không cần mount, không cần sudo — xem thẳng vào ảnh đĩa._

```bash
fdisk -l os.img                 # bảng phân vùng: thấy partition 1 bắt đầu ở sector 2048
mdir -i "os.img@@1M" -b -/ ::   # cây file bên trong FAT32
xxd -s 510 -l 2 os.img          # 2 byte cuối sector 0 phải là 55 aa
```

`mtools` (`mformat`/`mmd`/`mcopy`/`mdir`) đọc ghi FAT32 **ngay trên file ảnh**, không mount vào hệ thống.
Đó là lý do dựng ảnh trong WSL2 không cần quyền root — mount loop device trong WSL hay trục trặc.
