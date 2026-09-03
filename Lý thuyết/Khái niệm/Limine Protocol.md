---
tags: [concept, boot, spec]
---

# Limine Protocol

> Quy ước (ABI) để **bootloader bàn giao thông tin phần cứng cho kernel**. cavOS dùng Limine thay vì
> tự viết bootloader. Liên quan: [[Request-Response Mechanism]], [[HHDM]], [[Higher-Half Kernel]], [[Long Mode]].

## 1. Chuỗi boot
```
[Nguồn] → Firmware (UEFI/BIOS) → Bootloader (Limine) → Kernel (_start)
```
- **Firmware**: chạy đầu, init phần cứng cơ bản, nạp bootloader.
- **Bootloader (Limine)**: đọc file kernel, đưa CPU vào trạng thái phù hợp, nhảy vào `_start`.
- **Kernel**: code của bạn.

## 2. "Protocol" = hợp đồng giữa 2 chương trình
Bootloader và kernel do 2 bên viết → cần thống nhất: *kernel muốn gì* và *bootloader để thông tin ở đâu*.
Limine là một cách cụ thể (khác Multiboot/Multiboot2).

## 3. Cơ chế request/response → xem [[Request-Response Mechanism]]
Kernel để sẵn "phong bì có nhãn" (struct request, nhãn = dãy magic) trong nhị phân; Limine quét tìm
nhãn rồi điền con trỏ `response`.

## 4. Base revision — bắt tay phiên bản
```c
#define LIMINE_BASE_REVISION(N) \
    uint64_t limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) };
#define LIMINE_BASE_REVISION_SUPPORTED (limine_base_revision[2] == 0)
```
Kernel khai báo `{magic1, magic2, N=2}`. Nếu Limine **hỗ trợ** revision N, nó ghi phần tử thứ 3 = 0.
→ kiểm `LIMINE_BASE_REVISION_SUPPORTED`. Trong `kernel.c`:
```c
static volatile LIMINE_BASE_REVISION(2);
if (LIMINE_BASE_REVISION_SUPPORTED == false) panic();
```

## 5. Limine ĐÃ làm gì trước khi vào `_start`
| Đã thiết lập                                 | Kernel KHỎI phải tự làm                         |                                           |     |
| -------------------------------------------- | ----------------------------------------------- | ----------------------------------------- | --- |
| CPU ở [[Long Mode]]                          | 64-bit [[Long Mode|long mode]]                            | không cần code chuyển real→protected→long |     |
| Paging đã bật (bảng trang tạm)               | đã chạy ở địa chỉ ảo                            |                                           |     |
| [[HHDM]]: map toàn bộ RAM vật lý vào nửa cao | truy cập phys = `phys + hhdmOffset`             |                                           |     |
| Kernel nạp [[Higher-Half Kernel]]            | higher-half]], relocate ([[KASLR & PIE Kernel]] | KASLR                                     | —   |
| Stack sẵn sàng; các `response` đã điền       | chỉ việc đọc                                    |                                           |     |

→ Vì vậy [[Step 00 - Boot & Limine]] không có dòng asm bật long mode/paging nào.

## 5b. Limine KHÔNG làm gì (dễ hiểu nhầm)
Limine là **người đưa thư**, không phải người cấu hình phần cứng. Nó **chỉ đọc lại** thông tin firmware đã
dựng rồi giao cho kernel — KHÔNG tự tạo/sửa.

| Việc | Ai làm | Limine? |
|---|---|---|
| **Dựng memory map** (đánh dấu RAM/MMIO/reserved) | **Firmware** (UEFI/BIOS) lúc POST | ❌ chỉ **đọc lại** → trả qua `memmap` |
| **Enumerate PCI + gán BAR** (cấp MMIO cho thiết bị) | Firmware (boot device) / **Kernel** sau ([[Step 13 - PCI & NIC]]) | ❌ không đụng |
| **Framebuffer** | Firmware cấp qua GOP (đã gán BAR sẵn) | chỉ **lấy địa chỉ**, không tự gán |
| **ACPI tables** | Firmware tạo | ❌ chỉ trả con trỏ **RSDP** |

→ Các vùng `FRAMEBUF`/`RESERVED`/MMIO (vd `0xfd000000`, `0xb0000000`) thấy trong memmap là **di sản của
firmware**, có TRƯỚC khi Limine chạy — không phải Limine tạo. Triết lý bootloader: làm ít nhất có thể, chỉ đủ
để kernel tự chủ (nạp kernel + long mode + paging + HHDM + giao map). Mọi việc điều khiển thiết bị → kernel tự lo.

## 6. Các request cavOS dùng (`entry/bootloader.c`)
`paging_mode` (ép 4-level), `hhdm`, `kernel_address`, `memmap`, `smp`, `rsdp`.
Chi tiết: [[Step 02 - Bootloader Parser]].

## Tóm tắt 1 câu
Kernel "đặt hàng" thông tin qua struct request (nhãn magic); Limine "giao hàng" bằng cách điền
`response` trước khi nhảy vào `_start`, đồng thời đặt CPU vào long mode + paging + higher-half.

## 🔗 Spec
Limine `PROTOCOL.md` — *Base revision, Features, Paging Mode, HHDM, Kernel Address, Memory Map, SMP, RSDP*.
