---
tags: [lab, hands-on, video]
status: done
---

# Lab 0x02 — Framebuffer

> **Thực hành [[Step 03 - Framebuffer & Console]].** Lấy framebuffer từ Limine, tự vẽ pixel/hình/chữ —
> thấy tận mắt pixel format BGRX, pitch, và HHDM. Nối tiếp [[Lab 0x01 - Bootloader Parser]] (tái dùng serial).

**Vị trí:** `~/oskernel-lab/02-framebuffer/` · base revision **2** (khớp cavOS).

Source: `Thực hành/src/oskernel-lab/02-framebuffer/`.

> Chạy lại 2026-09-03 cho output **trùng từng dòng** với lần chạy 2026-06-01 bên dưới
> (fb `0xffff8000fd000000`, 1280x800, pitch 5120). Hợp lý: framebuffer là MMIO ở địa chỉ cố định,
> không đổi theo KASLR như `phys_base` của kernel ([[Lab 0x01 - Bootloader Parser]]).
**Chạy:** `make run` (serial headless) hoặc **`make run-gfx`** (thấy cửa sổ — cần X/WSLg).

## 📦 Cấu trúc module (tách từ Lab 0x02 trở đi)
```
io.h        — outb/inb (inline)
serial.{h,c}— UART 16550 (Step 01)
fb.{h,c}    — framebuffer: fb_init + put_pixel/fill_rect/draw_string + font 8×8
kernel.c    — kmain: orchestrate + in thông số + vẽ
```
`GNUmakefile`: `SRCS := kernel.c serial.c fb.c` (common.mk tự build từng `.o`). String output để **tiếng Anh**;
comment giải thích tiếng Việt.

## Làm gì
1. `fb_init()` khai `LIMINE_FRAMEBUFFER_REQUEST`, lấy `framebuffers[0]` → `address/width/height/pitch/bpp`.
2. `put_pixel(x,y,r,g,b)`: ghi 4 byte **BGRX**, **nhảy hàng bằng `pitch`** (không phải width*4).
3. Vẽ: nền xám → 3 thanh màu R/G/B → ô vuông trắng → chữ `HELLO FB` (font 8×8 tự nhúng, scale 4).
4. In thông số fb ra serial để đối chiếu Step 03.

## Khác với cavOS (Step 03)
- cavOS dùng macro `drawPixel` theo `fb.width` + font **PSF1** nạp từ `gohufont.h`. Lab dùng `put_pixel`
  theo **`pitch`** (đúng hơn khi có padding) + font 8×8 **nhúng thẳng** (vài ký tự, khỏi cần file PSF).
- Lab chỉ vẽ tĩnh; cavOS có cả console (con trỏ, scroll, `printf`→`putchar_`).

## Kết quả thật (QEMU `-M q35 -m 256M`, 2026-06-01)
```
=== Lab 0x02 - Framebuffer (Step 03) ===
[fb] address(VA, qua HHDM) = 0xffff8000fd000000
[fb] resolution = 1280 x 800
[fb] pitch = 5120 bytes/row   (width*4 = 5120)
[fb] bpp = 32
[fb] -> pitch == width*4 (no padding)
[fb] drawn: 3 color bars + white square + text 'HELLO FB'.
[done] halt.
```
(Độ phân giải mặc định QEMU q35 = **1280×800**; pitch = 5120 = width×4 → không padding.)
Màn hình (`run-gfx`): 3 thanh đỏ/lục/lam trên đỉnh, ô vuông trắng giữa, chữ vàng `HELLO FB`.

## 🔑 Quan sát đắt giá: HHDM được chứng minh
```
FB address (VA) = 0xffff8000fd000000
HHDM offset     = 0xffff800000000000   (Lab 0x01)
                ───────────────────────  trừ đi
FB phys (PA)    = 0x00000000fd000000
```
→ Đúng bằng entry `FRAMEBUFFER base=0xfd000000` trong **memmap dump của [[Lab 0x01 - Bootloader Parser]]**!
Tức `framebufferRes->address` Limine trả về **chính là `phys + hhdmOffset`** — ghi vào `fb.virt` = ghi VRAM
qua HHDM ([[HHDM]] mục "dùng VA nào"). Cùng một bài học, hai lab xác nhận chéo.

## ✅ Tự kiểm tra
1. Đổi `-m 256M` → `-m 512M`: địa chỉ fb có đổi không? (gợi ý: fb là MMIO, thường cố định ~`0xfd000000`).
2. Vì sao **phải** dùng `pitch` mà không `width*4`? Khi nào hai cái khác nhau? (xem [[Step 03 - Framebuffer & Console]] §3.2 cạm bẫy pitch).
3. Đảo thứ tự ghi `p[0]/p[1]/p[2]` (BGR→RGB) → màu 3 thanh đổi thế nào? (chứng minh format BGRX).
4. Bỏ kẹp biên trong `put_pixel` rồi vẽ `y ≥ height` → ghi tràn ngoài VRAM, chuyện gì xảy ra?

## Liên hệ
- Lý thuyết: [[Step 03 - Framebuffer & Console]] · nền tảng [[HHDM]].
- Build/os.img: [[Cẩm nang build & debug]].
