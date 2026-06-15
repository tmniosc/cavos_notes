---
tags: [lab, hands-on, boot]
status: done
---

# Lab 0x00 — Hello Serial

Tự viết lại phần **boot + serial** từ số 0 (gộp [[Step 00 - Boot & Limine]] + [[Step 01 - Serial UART]]).
Vị trí: `~/oskernel-lab`.

## Mục tiêu
- Limine nạp kernel ELF64 vào [[Long Mode]] → nhảy vào entry `kmain` (= `ENTRY(kmain)` trong `linker.ld`).
- Init COM1 (8N1, 115200) theo [[UART 16550]].
- In `"Hello Serial"` ra QEMU serial.

> ⚙️ **Base revision = 2** (khớp cavOS gốc, không dùng 3) — `LIMINE_BASE_REVISION(2)` trong `kernel.c`
> + comment `linker.ld`. Đã rebuild + boot QEMU lại OK (2026-05-31). Xem [[Limine Protocol]] §4.

## Các mảnh chính
- `linker.ld`: `ENTRY(kmain)` (lab dùng `kmain` làm entry, không phải `_start`), `. = 0xffffffff80000000`,
  PHDRS (text/rodata/data = `PT_LOAD`, **không ghi `FLAGS`** → ld tự suy W^X từ section flags — xem
  [[Step 00 - Boot & Limine]] §0.1.2), `. = ALIGN(CONSTANT(MAXPAGESIZE))` giữa các segment
  (xem [[Paging]] ý #3).
- `.limine_requests` gói trong `KEEP(*(.limine_requests))` (nằm trong `:rodata`) để Limine quét magic,
  `KEEP` chống linker GC bỏ section (chưa ai tham chiếu trực tiếp) — xem [[Request-Response Mechanism]].
- `limine.conf` + `limine bios-install` → đóng gói vào `os.img` (ảnh đĩa MBR + FAT32 + Limine,
  dựng bằng `scripts/mkimage.sh`) — **cấu trúc os.img**: xem [[Build & Debug Cheatsheet]].
- `serial.c`: `outb/inb`, init COM1, `serial_putc` chờ LSR bit5 (THRE).

## Kết quả
QEMU `-serial stdio` in ra `Hello Serial`. ✅

## 🔎 4 file phân tích build sinh sẵn (soi layout đã học)
Makefile lab tự tạo mỗi lần build — dùng để **thấy tận mắt** higher-half / ALIGN / W^X:

| File | Lệnh sinh | Cho biết |
|---|---|---|
| `kernel.map` | `ld -Wl,-Map=kernel.map` | **Layout sau link**: section ở địa chỉ nào, kích thước, mốc ALIGN, vị trí symbol |
| `kernel.dis` | `objdump -d -M intel` | disassembly |
| `kernel.sym` | `nm -n` | bảng symbol theo địa chỉ |
| `kernel.elf.txt` | `readelf -a` | toàn bộ ELF: program headers + `p_flags` (R/X…) |

> ⚠️ **Map file là sản phẩm của LINKER (`kernel.bin`), KHÔNG phải của `kernel.o`.** `.o` chưa link nên
> chưa có địa chỉ cuối để "map". Muốn soi section/flag của riêng `.o` (trước link):
> `readelf -S kernel.o` (→ `kernel.o.sections.txt`) hoặc `objdump -h kernel.o` (→ `kernel.o.headers.txt`)
> → thấy `.text`=AX, `.rodata`=A, `.data`=WA (gốc của W^X, xem [[Step 00 - Boot & Limine]] §0.1.2).

**Trích `kernel.map` (bằng chứng cho [[Paging]] ý #3):**
```
.text    0xffffffff80000000   0xf3            ← kernel ở higher-half
           0xffffffff80000060   kmain          ← entry point
         0xffffffff80001000  . = ALIGN(MAXPAGESIZE)   ← đẩy sang trang mới
.rodata  0xffffffff80001000   0x8a            ← rodata khởi đầu ĐÚNG biên trang 0x1000
```

## Liên hệ
- Lý thuyết: [[Step 00 - Boot & Limine]], [[Step 01 - Serial UART]].
- Build/QEMU: [[Build & Debug Cheatsheet]].
