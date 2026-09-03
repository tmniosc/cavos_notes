---
tags: [concept, memory, boot]
---

# HHDM — Higher Half Direct Map

> **Toàn bộ RAM vật lý được ánh xạ thẳng (tuyến tính) vào nửa cao của không gian ảo**, bắt đầu tại
> một offset cố định `hhdmOffset` do [[Limine Protocol|Limine]] cung cấp.

## Công thức vàng
```
địa_chỉ_ảo = địa_chỉ_vật_lý + hhdmOffset
địa_chỉ_vật_lý = địa_chỉ_ảo − hhdmOffset
```

## Vì sao cần?
Kernel chạy với [[Long Mode|paging]] đã bật → mọi truy cập là **địa chỉ ảo**. Nhưng kernel thường
phải đọc/ghi tại **địa chỉ vật lý** cụ thể (bảng trang, MMIO, cấu trúc ACPI...). HHDM cho phép truy
cập bất kỳ địa chỉ vật lý nào chỉ bằng cách cộng offset — không phải map tạm từng lần.

## Bằng chứng trong code (`entry/bootloader.c`)
```c
bootloader.hhdmOffset = limineHHDMres->offset;
...
// Limine trả RSDP ở dạng ảo; trừ offset để lưu địa chỉ VẬT LÝ:
bootloader.rsdp = (size_t)rsdp_response->address - bootloader.hhdmOffset;
```

## Sơ đồ
![[va-space-64bit.svg|1083]]

## CR3 vs HHDM — ai làm gì (đừng nhầm!)
HHDM **không thay thế** paging. CR3 + paging vẫn **luôn chạy**; HHDM chỉ là **một vùng được map sẵn
bên trong** page table mà CR3 trỏ tới.

| | CR3 + paging | HHDM |
|---|---|---|
| Bản chất | cơ chế **phần cứng** bắt buộc của [[Long Mode]] | một **quy ước map** (vùng VA = PA + offset) |
| Ai ép? | CPU — long mode không thể tắt paging | không ai; chỉ là cách Limine chọn map |
| Quan hệ | trỏ tới **cả** page table | **nằm trong** chính page table đó |

```
        CR3 ──► Page table (Limine dựng) ──► gồm: vùng HHDM + kernel + (sau: user/MMIO/heap)
```

- Khi kernel làm `*(phys + hhdmOffset)`: CPU **không** tự cộng offset — nó vẫn tra bảng qua CR3 như
  mọi VA. Công thức `+offset` đúng **chỉ vì** Limine đã điền entry: trang ảo `0xffff8000+N` → khung `N`.
- Limine "đơn giản hóa" = nó **dựng bảng + nạp CR3 hộ** trước `_start` → bạn không thấy `mov cr3` nào ở
  [[Step 00 - Boot & Limine]], nhưng CR3 *đang* hoạt động.
- **Bạn tự đụng CR3** ở [[Step 05 - Virtual Memory & Paging]]: dựng bảng riêng, nhớ map lại HHDM + kernel
  vào bảng mới **trước** khi `mov cr3`, kẻo kernel "mất chân".

Chi tiết cơ chế dịch: [[Paging]] (ý #1, #4 và ví dụ 4b).

## Dùng VA nào: kernel mapping vs HHDM
Kernel có **2 vùng VA** (cùng phủ RAM nhưng khác mục đích & quyền — đây là aliasing 2 VA → 1 PA):

| VA                 | Dải             | Dùng cho                                               | Quyền     | Cách dùng                                  |
| ------------------ | --------------- | ------------------------------------------------------ | --------- | ------------------------------------------ |
| **Kernel mapping** | `0xffffffff80…` | **chạy code** + biến global (.text/.rodata/.data/.bss) | R+X / R+W | **ngầm**, compiler tự sinh — không gọi tên |
| **HHDM**           | `0xffff8000…`   | **đọc/ghi 1 PA bất kỳ** như dữ liệu                    | R+W, NX   | **chủ động** `pa + hhdmOffset`             |

```c
void *pa = pmm_alloc();                 // PMM trả PA thuần, vd 0x0ffb5000
// *(uint64_t*)pa = 1;   ❌ PA KHÔNG phải VA — kernel chạy với paging, mọi deref là VA
uint64_t *p = (uint64_t*)((size_t)pa + hhdmOffset);  // ✅ đổi sang VA qua HHDM
*p = 123;                               // giờ ghi được
```

**Khi nào cần HHDM** (càng về sau càng nhiều): PMM ghi vào frame vừa cấp ([[Step 04 - Physical Memory Manager]]);
sửa page table — entry chứa PFN vật lý ([[Step 05 - Virtual Memory & Paging]]); đọc RSDP/MADT ([[Step 07 - ACPI]]);
chạm MMIO/framebuffer ([[Step 03 - Framebuffer & Console]]).

> 📌 Quy tắc: **code kernel sống ở kernel mapping (cần X); cầm một PA muốn đọc/ghi → qua HHDM (W, NX).**
> Đó là lý do aliasing hữu ích: cùng byte, một cửa để *chạy*, một cửa để *ghi*.

Liên quan: [[Higher-Half Kernel]], [[Step 04 - Physical Memory Manager]], [[Step 05 - Virtual Memory & Paging]].
