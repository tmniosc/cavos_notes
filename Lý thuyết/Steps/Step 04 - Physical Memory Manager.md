---
tags: [step, memory]
status: done
---

# Step 04 — Physical Memory Manager (PMM)

**Files:** `memory/pmm.c`, `utilities/data_structures/bitmap.c`, `include/bitmap.h`, `include/pmm.h`
**Đầu vào:** `memmap` + `mmTotal` + `hhdmOffset` từ [[Step 02 - Bootloader Parser]]
**Nền tảng:** [[Paging]] (frame 4 KiB = đơn vị cấp phát), [[HHDM]] (truy cập bitmap qua HHDM)

## 🎯 Mục đích
**Biết frame RAM vật lý nào đang trống để cấp phát** — "người gác kho" RAM ở mức thô nhất.
```
memmap (Limine)          PMM                       người dùng
"RAM nào tồn tại"  ──►  "RAM nào CÒN TRỐNG"  ──►  PhysicalAllocate() → 1 frame
   (tĩnh)               (động, theo dõi)          PhysicalFree()
```
memmap chỉ là thông tin **tĩnh**; PMM **theo dõi động** ai đang dùng gì. Không có PMM thì không cấp phát
động được → mọi tầng sau đều bắt đầu bằng `PhysicalAllocate()`: page table [[Step 05 - Virtual Memory & Paging]],
heap/malloc, stack & pagedir mỗi task [[Step 11 - Multitasking & Scheduler]], buffer driver/FS. PMM chỉ quản
**frame thô** (theo PA), không biết VA — "đặt frame ở VA nào" là việc của [[Step 05 - Virtual Memory & Paging]].

### 🧒 Nói thật đơn giản
PMM giống một **quyển sổ giữ kho RAM**. Hình dung RAM bị chia thành hàng triệu **ô vuông 4 KiB** (gọi là
*frame*). PMM làm đúng 2 việc:

1. **Ghi sổ trạng thái từng ô**: ô nào *đang dùng*, ô nào *còn trống*. Mỗi ô chỉ cần **1 bit** (1 = dùng,
   0 = trống) → cả quyển sổ gọi là **bitmap**.
2. **Cấp & thu hồi ô**: ai cần nhớ thì hỏi "cho tôi 1 ô trống" (`PhysicalAllocate`), trả lại thì "ô này
   xong rồi" (`PhysicalFree`). PMM tra sổ, đánh dấu, trả về.

```
RAM:     [ô0][ô1][ô2][ô3][ô4]...   ← mỗi ô = 1 frame 4 KiB
sổ:        1   0   0   1   0  ...   ← 1=dùng, 0=trống
                   ▲
            "cho 1 ô trống" → PMM thấy ô2 trống → đánh dấu 1, trả PA của ô2
```

Cả bài diễn ra thế này:
- **Tính sổ cần bao to**: RAM tổng chia cho 4 KiB ra số ô → cần ngần ấy bit.
- **Tìm chỗ đặt sổ**: chọn vùng RAM trống đầu tiên đủ lớn, đặt quyển sổ vào đó.
- **Khởi tạo "đóng hết rồi mở"**: ban đầu đánh dấu *mọi* ô là "đã dùng" (an toàn), rồi chỉ **mở** đúng các
  vùng Limine báo là dùng được. Cách này không bao giờ lỡ cấp nhầm vùng cấm.
- **Tự đánh dấu sổ**: vì quyển sổ cũng nằm trong RAM, PMM đánh dấu chỗ đó "đã dùng" kẻo cấp đè lên chính nó.
- **Cấp/thu hồi**: muốn ô trống thì PMM quét sổ tìm ô đầu tiên còn `0`, đánh dấu rồi đưa cho.

> ⚠️ 2 điều người mới hay nhầm:
> - **PMM không lưu *nội dung* của RAM**, chỉ lưu *trạng thái* (1 bit: trống/dùng). Trong ô có gì nó không quan tâm.
> - **Bản thân quyển sổ cũng phải nằm trong RAM** → phải tìm chỗ đặt + tự đánh dấu "đã dùng". Đây là chi tiết
>   kỹ thuật, không phải mục đích chính.

## 4.1 Ý tưởng: bitmap 1 bit / frame
> _1 bit theo dõi 1 ô RAM 4 KiB. Cần bao nhiêu bit = tổng RAM chia cho 4 KiB._

```
BLOCK_SIZE = 4096 (= PAGE_SIZE)   BLOCKS_PER_BYTE = 8
bit = 1 → frame ĐÃ DÙNG (used)    bit = 0 → frame TRỐNG (free)
```
Quy ước cavOS: `10000000 → frame 0 dùng, còn lại free` (comment đầu `bitmap.c`).

Công thức kích thước bitmap (`initiatePMM`):
```c
BitmapSizeInBlocks = DivRoundUp(mmTotal, BLOCK_SIZE);  // tổng RAM / 4KiB = số frame
BitmapSizeInBytes  = DivRoundUp(BitmapSizeInBlocks, 8);// mỗi byte chứa 8 frame
```
`mmTotal` tính ở `bootloader.c` = **cộng dồn `length` các vùng KHÁC `RESERVED`** (không lấy "địa chỉ cuối
cao nhất") → bỏ MMIO/PCI hole ở địa chỉ rất cao, bitmap không phình theo lỗ. Xem [[Lab 0x03 - PMM & VMM]].

## 4.1b Bộ nhớ vật lý KHÔNG liền mạch (vì sao cần memmap)
> _RAM không phải một dải liền từ 0. Không gian địa chỉ vật lý là bản đồ chung; RAM chỉ chiếm vài mảnh, xen
> kẽ là vùng cấm (BIOS/ACPI) và MMIO (thiết bị). Đó là lý do PMM phải đọc memmap thay vì "RAM = 0..tổng"._

![[phys-mem-not-contiguous.svg]]
- **RAM rải rác, có lỗ** (BIOS ~`0x9fc00`, PCI hole quanh `0xfd000000`).
- **Địa chỉ cao ≠ RAM**: `0xfd000000` (framebuffer) và `0xfd00000000` là **MMIO** — xem [[Step 03 - Framebuffer & Console]], [[HHDM]].
- → Đây là lý do PMM **không cấp theo địa chỉ liền** mà tra `memmap` (USABLE) cho từng vùng; và tính `mmTotal`
  bằng cộng dồn (loại RESERVED) thay vì max-end.

## 4.2 `initiatePMM()` — 5 bước dựng bitmap
> _Trình tự khởi tạo quyển sổ: tính cỡ → tìm chỗ đặt → đóng hết → mở vùng dùng được → tự đánh dấu sổ._

```c
1) Tính size bitmap từ mmTotal.
2) Tìm vùng USABLE ĐẦU TIÊN đủ lớn chứa bitmap:
     if (entry->type != USABLE || entry->length < BitmapSizeInBytes) continue;
3) Đặt bitmap NGAY TRONG vùng đó, truy cập qua HHDM:
     bitmapStartPhys = mm->base;
     physical.Bitmap = (uint8_t *)(bitmapStartPhys + bootloader.hhdmOffset);  // ← HHDM!
4) Khởi tạo trạng thái:
     memset(Bitmap, 0xff, size);                  // MẶC ĐỊNH: mọi frame = used (an toàn)
     for each USABLE entry:     MarkRegion(..., 0);// mở (free) đúng vùng usable
     for each non-USABLE entry: MarkRegion(..., 1);// đóng lại reserved/ACPI/FB...
5) MarkRegion(bitmapStartPhys, BitmapSizeInBytes, 1); // tự đánh dấu CHÍNH bitmap là used
   bitmap->ready = true;
```

> 🔑 **3 điểm tinh tế:**
> - **Bitmap tự host trong RAM nó quản lý** — không cần allocator khác. Đặt ở vùng usable đầu đủ lớn,
>   rồi *bước 5* tự đánh dấu chỗ đó là used để không cấp phát đè lên chính nó.
> - **`memset 0xff` (used) trước, mở usable sau**: mặc định đóng tất → chỉ mở đúng phần Limine báo
>   USABLE → mọi vùng lạ/hole đều an toàn (coi như used, không bao giờ cấp).
> - **Bitmap truy cập qua HHDM** (`base + hhdmOffset`): đúng bài [[HHDM]] — cầm PA, đọc/ghi qua VA HHDM.

## 4.3 API cấp phát (`pmm.c`)
> _2 hàm cả kernel dùng: xin frame và trả frame. Trả về địa chỉ VẬT LÝ._

```c
size_t PhysicalAllocate(int pages);        // trả PA (vật lý) của vùng pages frame liên tiếp
void   PhysicalFree(size_t ptr, int pages);// trả lại
```
- Có **spinlock** `LOCK_PMM` → an toàn SMP.
- `PhysicalAllocate` **panic nếu hết RAM** (không trả NULL âm thầm).
- ⚠️ Trả về **địa chỉ VẬT LÝ**. Muốn đọc/ghi nội dung frame → phải `+ hhdmOffset` ([[HHDM]] "dùng VA nào").

## 4.4 Cơ chế bitmap (`bitmap.c`)
> _Các thao tác trên quyển sổ: bật/tắt 1 bit, đổi giữa "địa chỉ" và "số ô", tìm dải ô trống, đánh dấu cả vùng._

**Get/Set 1 bit:**
```c
BitmapGet(b, block): byte = block/8; offset = block%8; return (Bitmap[byte] >> offset) & 1;
BitmapSet(b, block, v): bật/tắt bit (1<<offset) trong Bitmap[byte];
```
**Đổi địa chỉ ↔ block:** `ToPtr = mem_start + block*BLOCK_SIZE`; `ToBlock = (ptr - mem_start)/BLOCK_SIZE`.
> `physical` là biến global (`pmm.h`) → zero-init → **`mem_start = 0`**. Vậy block đếm thẳng từ **địa chỉ
> vật lý 0** (`block = PA / 4096`). Đó là lý do `PhysicalAllocate` trả về PA dùng được trực tiếp.

**`FindFreeRegion(blocks)`** — quét tìm dải `blocks` bit-0 liên tiếp (first-fit):
```c
duyệt từ lastDeepFragmented:
  gặp bit 1 (used) → reset đếm, start = i+1
  gặp bit 0 (free) → size++; nếu size >= blocks → trả start
không thấy → INVALID_BLOCK
```
- **Tối ưu `lastDeepFragmented`**: nhớ vị trí free thấp nhất để lần sau khỏi quét lại từ đầu (vì cấp phát
  1-frame là phổ biến nhất). Khi `Free` ở block thấp hơn → kéo mốc này xuống.

**`MarkRegion(base, sizeBytes, isUsed)`** — đánh dấu cả vùng:
- `isUsed=1`: làm tròn **xuống** (`ToBlock`) + size **lên** → phủ trọn (không sót byte used).
- `isUsed=0`: làm tròn **lên** (`ToBlockRoundUp`) + size **xuống** → free **bảo thủ** (không lỡ mở nhầm
  frame chỉ free một phần). → Đây là lý do có 2 nhánh round khác nhau.

## 4.5 Bức tranh tổng thể
> _Toàn cảnh: RAM chia vùng (dùng được / cấm), bitmap đánh dấu tương ứng, cấp phát lấy từ vùng trống._

![[pmm-bitmap-mapping.svg]]

## ✅ Câu hỏi mở (đã trả lời)
- **Bitmap đặt ở đâu?** → ngay trong vùng USABLE đầu tiên đủ lớn (`mm->base`), truy cập qua HHDM; tự
  MarkRegion chính nó là used.
- **Vùng reserved/ACPI/FB xử lý sao?** → `memset 0xff` (mặc định used) rồi chỉ mở USABLE; non-USABLE
  được Mark lại used lần nữa cho chắc.

## Liên hệ
- PMM cấp **frame vật lý** → [[Step 05 - Virtual Memory & Paging]] dùng làm page table + map VA→PA.
- `bitmap.c` là data structure tổng quát (`DS_Bitmap`) — còn dùng cho ext2 ([[Step 14 - AHCI & Filesystems]]).

## Ánh xạ spec
- Limine memmap types (USABLE/RESERVED/ACPI/BOOTLOADER_RECLAIMABLE/KERNEL_AND_MODULES/FRAMEBUFFER): [[Danh mục tài liệu]].
