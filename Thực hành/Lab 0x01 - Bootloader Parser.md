---
tags: [lab, hands-on, boot]
status: done
---
# Lab 0x01 — Bootloader Parser

> **Thực hành [[Step 02 - Bootloader Parser]].** Tự khai các Limine request, đọc response,
> in ra serial — thấy tận mắt paging/HHDM/kernel-address/memmap mà Step 02 chỉ đọc vào struct.
> Nối tiếp [[Lab 0x00 - Hello Serial]] (tái dùng serial 16550).

**Vị trí:** `~/oskernel-lab/01-bootloader-parser/` · build/run y hệt Lab 0x00 (`make kernel`, `make run`).

Source: `Thực hành/src/oskernel-lab/01-bootloader-parser/`.

## Làm gì
Khai 4 request (gói trong `.limine_requests`, `used`):
```c
#define LIMINE_REQUEST __attribute__((used, section(".limine_requests")))
LIMINE_REQUEST static volatile struct limine_hhdm_request           hhdm_request   = {.id=LIMINE_HHDM_REQUEST};
LIMINE_REQUEST static volatile struct limine_kernel_address_request kaddr_request  = {.id=LIMINE_KERNEL_ADDRESS_REQUEST};
LIMINE_REQUEST static volatile struct limine_memmap_request         memmap_request = {.id=LIMINE_MEMMAP_REQUEST};
LIMINE_REQUEST static volatile struct limine_paging_mode_request    paging_request = {.id=LIMINE_PAGING_MODE_REQUEST, .mode=LIMINE_PAGING_MODE_X86_64_4LVL};
```
Trong `kmain`: kiểm `response != NULL` rồi in:
- **paging** mode (4-level / 5-level),
- **hhdm** offset (hex),
- **kernel** virt_base + phys_base (hex) → đúng câu "ảo ở `…80…`, vật lý ở đâu",
- **memmap**: từng entry `base/len/type` + tổng `USABLE` và `non-RESERVED` (MiB).

Thêm 2 helper so với Lab 0x00: `serial_puthex(uint64_t)`, `serial_putdec(uint64_t)`.

## Kỳ vọng output (QEMU `-m 256M`)
```
[paging] mode = 4-level
[hhdm]   offset = 0xffff800000000000        (cố định cho 4-level; KHÔNG đổi theo KASLR)
[kernel] virt = 0xffffffff80000000          (cố định: top-2GiB của higher-half)
         phys = 0x0000000000xxxxxx          ← phys thật Limine chọn (đổi mỗi lần boot)
[memmap]
  [0] base=0x... len=0x... USABLE
  ...
  -- USABLE = ~2xx MiB
```

## Khác với cavOS (Step 02)
- cavOS gom vào struct `bootloader` dùng nội bộ; lab chỉ **in ra** để quan sát.
- Lab **chưa** trừ `hhdmOffset` cho RSDP / chưa xử SMP (giữ tối giản); tập trung HHDM + memmap + kernel-address.
- Khẳng định lại bài "ảo vs vật lý": `virt_base` luôn `0xffffffff80000000`, `phys_base` đổi theo mỗi lần boot.

## ✅ Tự kiểm tra
1. `hhdm offset + phys_base` có bằng `virt_base` không? Vì sao (gần) bằng?
2. Vì sao `USABLE` < tổng RAM khai báo cho QEMU? (vùng RESERVED/BOOTLOADER/KERNEL ăn bớt)
3. Đổi `-m 256M` → `-m 512M` trong `QEMU_FLAGS`, output USABLE đổi thế nào?

## 📝 Notes của tôi

### Run thật (2026-05-28, QEMU `-M q35 -m 256M`)
```
=== Lab 0x01 - Bootloader Parser (Step 02) ===
[paging] mode = 4-level
[hhdm]   offset = 0xffff800000000000
[kernel] virt = 0xffffffff80000000
         phys = 0x000000000ffb2000
[memmap]                                         ; 16 entries
  [0] base=0x0000000000001000 len=0x0000000000005f000 BOOT_RECLAIM
  [1] base=0x0000000000060000 len=0x000000000003f000 USABLE
  [4] base=0x0000000000100000 len=0x000000000fe58000 USABLE   ← khối RAM "thật" sau 1 MiB
  [8] base=0x000000000ffb2000 len=0x0000000000002000 KERNEL_MODS  ← chính là phys_base
  [12] base=0x00000000fd000000 len=0x00000000003e8000 FRAMEBUFFER
  ...
  -- USABLE = 254 MiB / non-RES = 259 MiB (QEMU cấp 256 MiB)
[done] halt.
```

### Run thật 2026-09-03 (QEMU `-M q35 -m 256M`)
```
=== Lab 0x01 - Bootloader Parser (Step 02) ===
[paging] mode = 4-level
[hhdm]   offset = 0xffff800000000000        ← Y HỆT lần trước: HHDM cố định, không theo KASLR
[kernel] virt = 0xffffffff80000000          ← Y HỆT: top-2GiB luôn cố định
         phys = 0x000000000ff42000          ← ĐỔI (lần trước 0x0ffb2000) — Limine chọn lại mỗi lần boot
[memmap]                                         ; 16 entries
  [0]  base=0x0000000000001000 len=0x0000000000052000 BOOT_RECLAIM
  [1]  base=0x0000000000053000 len=0x000000000004c000 USABLE
  [4]  base=0x0000000000100000 len=0x000000000fe3f000 USABLE   ← khối RAM chính sau 1 MiB
  [6]  base=0x000000000ff42000 len=0x0000000000003000 KERNEL_MODS  ← đúng bằng phys_base
  [12] base=0x00000000fd000000 len=0x00000000003e8000 FRAMEBUFFER
  ...
  -- USABLE = 254 MiB / non-RES = 259 MiB
[done] halt.
```
> **Điều đáng học nằm ở chỗ so 2 lần chạy:** `hhdm` và `virt` **không đổi một bit**, còn `phys_base` +
> ranh giới các vùng thì **đổi hết**. Đó chính là lý do kernel phải hỏi bootloader thay vì hard-code địa chỉ.
> Vẫn đúng 16 entry và vẫn `254 / 259 MiB` vì QEMU cấu hình y như cũ.

### 🏷️ Các loại vùng (memmap type) — kernel được làm gì với chúng
> _Mỗi vùng có 1 "nhãn" cho biết là RAM trống, RAM mượn-tạm, hay vùng cấm. PMM ([[Step 04 - Physical Memory Manager]]) dựa vào nhãn này để quyết định cấp phát._

| type                                      | Là RAM?            | Cấp phát?           | Chứa gì                                                                                                 |
| ----------------------------------------- | ------------------ | ------------------- | ------------------------------------------------------------------------------------------------------- |
| **USABLE**                                | ✅                  | ✅ ngay              | RAM trống hoàn toàn — nguồn chính của PMM                                                               |
| **BOOT_RECLAIM** (Bootloader Reclaimable) | ✅                  | ✅ *sau khi reclaim* | cấu trúc tạm Limine: page table cũ (PML4 lúc boot), boot info, response struct. Đọc xong → đòi lại được |
| **RESERVED**                              | ❌ (thường là MMIO) | ❌ không bao giờ     | firmware/BIOS, **MMIO/PCI hole** (vd framebuffer, `0xfd00000000`), lỗ địa chỉ không có RAM              |
| **KERNEL_MODS** (Kernel & Modules)        | ✅                  | ❌                   | nơi kernel image được nạp — đang chạy, cấm đụng (`phys_base` rơi vào đây)                               |
| **FRAMEBUFFER**                           | ❌                  | ❌                   | VRAM màn hình (MMIO) — xem [[Step 03 - Framebuffer & Console]]                                          |
| **ACPI_RECLAIM**                          | ✅                  | ✅ *sau reclaim*     | bảng ACPI; đọc xong reclaim được (giống BOOT_RECLAIM)                                                   |
| **ACPI_NVS** / **BAD_MEM**                | ✅/—                | ❌                   | ACPI non-volatile (giữ nguyên) / RAM hỏng                                                               |

→ **2 mức phân loại PMM dùng** ([[Step 04 - Physical Memory Manager]]):
- **"Đếm vào tổng RAM" (`mmTotal`)**: mọi type **trừ `RESERVED`** (cộng dồn length).
- **"Cấp phát được ngay"**: `USABLE` (+ `BOOT_RECLAIM` nếu coi như đã reclaim). Dòng `-- USABLE` vs `non-RES`
  ở trên chính là 2 con số này (254 vs 259 MiB).

Đây là **Limine memmap types** (`LIMINE_MEMMAP_USABLE`, `..._BOOTLOADER_RECLAIMABLE`, `..._RESERVED`... trong `limine.h`).

### 🗺️ Sơ đồ: 4 con số này nằm đâu trong KHÔNG GIAN ẢO, map về RAM nào
> _hhdm/kernel-virt/kernel-phys/memmap không phải số trừu tượng — mỗi vùng RAM vật lý (cột trái) được trỏ
> tới từ những địa chỉ ảo nào (2 cột phải). Số thật của lần chạy trên: `phys = 0x0ffb2000`._

```
  PA (vật lý)  | VA qua HHDM ②      | VA qua kernel map ③ | ghi chú
  -------------+--------------------+---------------------+---------------------------
  0x00000000   | 0xffff800000000000 | -                   | mốc HHDM (= PA + offset)
  0x00100000   | 0xffff800000100000 | -                   | khối RAM chính (chỉ HHDM)
  0x0ffb2000   | 0xffff80000ffb2000 | 0xffffffff80000000  | KERNEL -> 2 VA = ALIASING
  0xfd000000   | 0xffff8000fd000000 | -                   | FRAMEBUFFER/MMIO (ko phải RAM)

  ② cột HHDM : MỌI vùng đều có (= PA + hhdmOffset) — Limine map toàn bộ RAM vào nửa cao.
  ③ cột kernel: CHỈ vùng KERNEL có (0xffffffff80000000) — map riêng kernel image; vùng khác '-'.
```
→ Vùng KERNEL `0x0ffb2000` là dòng DUY NHẤT có cả 2 cột VA → **2 địa chỉ ảo cùng trỏ 1 PA** = aliasing:
một "cửa" qua HHDM (đọc/ghi như data), một "cửa" qua kernel map (chạy code, quyền X). Xem [[HHDM]] "dùng VA nào".

### Trả lời ✅ Tự kiểm tra
1. **`hhdm + phys_base = virt_base?` KHÔNG.** `0xffff800000000000 + 0x0ffb2000 = 0xffff80000ffb2000`, nhưng kernel virt = `0xffffffff80000000` — hai vùng higher-half **khác nhau**: HHDM map toàn bộ RAM vật lý vào `0xffff8000…`, còn kernel có **ánh xạ riêng** ở top-2 GiB (`0xffffffff80…`). Tức là một byte trong kernel có **hai địa chỉ ảo cùng trỏ về cùng phys**: qua HHDM và qua kernel mapping (xem sơ đồ trên — đây là **aliasing**, [[HHDM]]).
2. `USABLE 254 MiB < 256 MiB QEMU cấp` vì các vùng `RESERVED/BOOT_RECLAIM/KERNEL_MODS/FRAMEBUFFER` ăn bớt (entry 0/2/3/5/7/8/9/10 trong dump trên).

> 📊 **Thấy 3 cột địa chỉ (PA / VA-HHDM / VA-kernel) thành bảng**: [[Lab 0x03 - PMM & VMM]] có `pmm_dump_map`
> in mọi vùng kèm cả 2 cột VA — đúng sơ đồ này nhưng đầy đủ 16 vùng.

### Gotcha khi chạy
- `make image` **chỉ dựng `os.img`** rồi dừng — thấy stderr của `limine bios-install` (`Physical block size... / Installing to MBR / Reminder: copy limine-bios.sys / installed successfully`) là **bình thường**, không phải lỗi. Dòng `Reminder` Limine in cứng mọi lần, nhưng `scripts/mkimage.sh` đã chép `limine-bios.sys` vào `::/boot/limine/` ở bước `5/5` → boot OK.
- Muốn thấy serial của kernel: dùng **`make run`** (không phải `make image`).
