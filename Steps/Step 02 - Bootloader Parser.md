---
tags: [step, boot]
status: done
---

# Step 02 — Bootloader Parser

**File:** `entry/bootloader.c`, `include/bootloader.h` · **Spec:** [[Limine Protocol]] · KN: [[Request-Response Mechanism]], [[HHDM]]
**🔬 Thực hành:** [[Lab 0x01 - Bootloader Parser]] — tự khai HHDM/kernel_address/memmap/paging request, in response ra serial.

## 🎯 Mục đích
**Thu hoạch mọi thông tin phần cứng Limine cung cấp vào MỘT struct dùng chung**, để các step sau khỏi
đụng trực tiếp vào API Limine. Limine đã điền các `response` (HHDM, kernel address, memmap, SMP, RSDP...);
step này đọc hết và đóng gói gọn vào `bootloader`. Đây là **nguồn dữ liệu đầu vào** cho gần như mọi tầng:
PMM cần `memmap`+`mmTotal`, paging cần `hhdmOffset`, ACPI cần `rsdp`, scheduler cần `smp`.

### 🧒 Nói thật đơn giản
Lúc giao máy, Limine có kèm theo một "tờ khai" về phần cứng. Step 02 **đọc tờ khai đó và chép vào một cuốn
sổ tay** (`struct bootloader`) để cả kernel về sau tra cứu. Không tính toán phức tạp — chỉ **thu thập thông
tin** một lần, gọn vào một chỗ.

6 thứ thu thập, và sau này ai cần:
- **Tổng RAM + bản đồ RAM** (vùng nào dùng được / cấm) → cho bộ quản lý RAM [[Step 04 - Physical Memory Manager]].
- **Độ lệch HHDM** (con số để đổi giữa "địa chỉ thật" và "địa chỉ kernel dùng") → cho [[Step 05 - Virtual Memory & Paging]].
- **Kernel nằm ở đâu** (địa chỉ ảo & vật lý).
- **Có mấy CPU** → cho đa nhiệm [[Step 11 - Multitasking & Scheduler]].
- **Bảng ACPI ở đâu** (thông tin bo mạch) → cho [[Step 07 - ACPI]].

Ví von: Limine để lại cho bạn một tập giấy tờ; bài này gom hết vào một bìa hồ sơ để tiện lấy ra dùng.

`initialiseBootloaderParser()` chạy ngay sau [[Step 01 - Serial UART|serial]] trong `_start()`.
**Nhiệm vụ:** thu hoạch mọi `response` Limine đã điền → gom vào **một struct `bootloader`** dùng chung cả kernel.
Đây là lúc [[Request-Response Mechanism]] "trả công": Step 00 chỉ *khai báo* request, giờ mới *đọc* response.

## 6 request được khai báo
> _6 "phiếu yêu cầu" kernel đặt sẵn, mỗi phiếu hỏi một thứ (RAM, màn hình, số CPU...). Limine thấy phiếu thì
> điền câu trả lời vào ô `response`._

```c
static volatile struct limine_paging_mode_request liminePagingreq =
    {.id = LIMINE_PAGING_MODE_REQUEST, .revision = 0, .mode = LIMINE_PAGING_MODE_X86_64_4LVL};
static volatile struct limine_hhdm_request           limineHHDMreq = {.id = LIMINE_HHDM_REQUEST, ...};
static volatile struct limine_kernel_address_request limineKrnreq  = {.id = LIMINE_KERNEL_ADDRESS_REQUEST, ...};
static volatile struct limine_memmap_request         limineMMreq   = {.id = LIMINE_MEMMAP_REQUEST, ...};
static volatile struct limine_smp_request            limineSmpReq  = {.id = LIMINE_SMP_REQUEST, ...};
static volatile struct limine_rsdp_request           limineRsdpReq = {.id = LIMINE_RSDP_REQUEST, ...};
```
Mỗi cái = `.id` (magic) + `.response = NULL`; bootloader quét magic rồi điền `.response`.

## Struct đích `bootloader` (`bootloader.h`)
> _"Cuốn sổ tay" chứa toàn bộ thông tin đã gom: mỗi dòng là một mẩu (RAM ở đâu, kernel chỗ nào...). Cả kernel
> về sau tra sổ này thay vì hỏi lại Limine._

```c
typedef struct Bootloader {
  size_t   hhdmOffset;                     // ② ảo↔vật lý
  size_t   kernelVirtBase, kernelPhysBase; // ③ kernel ở đâu (KASLR)
  size_t   rsdp;                           // ⑥ ACPI (phys)
  size_t   mmTotal;                        // ④ tổng RAM (loose)
  uint64_t mmEntryCnt;
  struct limine_memmap_entry **mmEntries;  // ④ bản đồ RAM
  struct limine_smp_response  *smp;        // ⑤ danh sách CPU
  uint64_t smpBspIndex;
} Bootloader;
```

## 6 thứ thu hoạch
> _Bảng tra nhanh: mỗi phiếu lấy ra cái gì và sau này bài nào dùng tới._

| #   | Request        | Lấy ra                            | Dùng cho                               |
| --- | -------------- | --------------------------------- | -------------------------------------- |
| ①   | paging_mode    | xác nhận 4-level                  | [[Step 05 - Virtual Memory & Paging]]  |
| ②   | hhdm           | `hhdmOffset` (`VA = PA + offset`) | [[HHDM]], mọi truy cập PA              |
| ③   | kernel_address | virt/phys base kernel             | [[KASLR & PIE Kernel]]                 |
| ④   | memmap         | mảng vùng RAM (usable/reserved…)  | [[Step 04 - Physical Memory Manager]]  |
| ⑤   | smp            | danh sách CPU + BSP index         | [[Step 11 - Multitasking & Scheduler]] |
| ⑥   | rsdp           | con trỏ RSDP (ACPI)               | [[Step 07 - ACPI]]                     |

### Các con số này nghĩa là gì (toạ độ trong không gian ảo + map về RAM)
- **② hhdmOffset** (`0xffff800000000000`): mốc vùng HHDM. Cộng vào PA bất kỳ → ra VA đọc/ghi được ngay ([[HHDM]]).
- **③ kernel_address**: kernel có **virt** (`0xffffffff80000000`, cố định) và **phys** (đổi mỗi boot — KASLR).
  Cùng frame kernel ⇒ **2 VA** (qua HHDM + qua kernel mapping) trỏ **1 PA** → aliasing.
- **④ memmap**: bảng liệt kê từng vùng RAM vật lý (cấp được / cấm) → đầu vào PMM ([[Step 04 - Physical Memory Manager]]).
- **⑥ rsdp**: là **PA thuần**; muốn đọc bảng ACPI phải `+ hhdmOffset` trước ([[Step 07 - ACPI]]).

> 🗺️ **Sơ đồ "RAM vật lý ← → địa chỉ ảo"** (mũi tên aliasing, số thật): xem [[Lab 0x01 - Bootloader Parser]].
> Bảng đầy đủ 3 cột PA / VA-HHDM / VA-kernel cho mọi vùng: [[Lab 0x03 - PMM & VMM]] (`pmm_dump_map`).

## Lưu ý
> _Vài điều dễ sụp nếu quên: phải kiểm tra Limine có trả lời chưa trước khi đọc._

- Luôn check `req.response != NULL` trước khi đọc (Limine có thể không cấp).
- `memmap` là **đầu vào sống còn** cho PMM: chỉ các entry `LIMINE_MEMMAP_USABLE` mới được cấp phát.
- `hhdmOffset` điển hình `0xffff800000000000` — xem [[Paging]] ý #5.

## Ánh xạ spec
- Limine protocol (struct request/response, memmap types): [[Spec Library]].
- ACPI RSDP: [[Step 07 - ACPI]], spec ACPI.
