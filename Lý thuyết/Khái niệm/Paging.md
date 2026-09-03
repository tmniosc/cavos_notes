---
tags: [cavos, paging, mmu, x86_64, nen-tang]
status: done
created: 2026-05-31
---

# Paging — đủ dùng để đọc cavOS

> Note nền tảng "just-in-time": nạp **5 ý cốt lõi** để đọc `link.ld`, `memmap`, `HHDM` không bị bí.
> KHÔNG đào sâu Intel SDM ở đây — để dành tới step VMM/paging thật trong cavOS.
> Liên kết: [[Step 00 - Boot & Limine]], [[Step 02 - Bootloader Parser]], [[HHDM]], [[Higher-Half Kernel]], [[Home]].

Tiến độ: [[Home]] · phục vụ cho [[Step 03 - Framebuffer & Console]] và các step sau.

---

## TL;DR — học gì, để dành gì

| Nên nạp NGAY (1–2 buổi)              | Để DÀNH (tới step VMM)                  |
| ------------------------------------ | --------------------------------------- |
| 5 ý cốt lõi bên dưới                 | Toàn bộ Intel SDM Vol.3 Ch.4            |
| OSDev "Paging" (lướt)                | Chi tiết từng bit PTE, PAT, global page |
| OSTEP ch. Address Translation/Paging | TLB shootdown, hugepage 2M/1G           |

**Nguyên tắc:** đọc code cavOS trước → gặp khái niệm lạ → tra đúng lúc → ghi vào vault. Đừng học trọn môn HĐH trước.

---

## 1. Địa chỉ ảo → vật lý: CPU tự dịch qua một bảng

Mọi địa chỉ kernel chạm vào là **địa chỉ ảo (virtual address — VA)**. Phần cứng MMU dịch sang **địa chỉ vật lý (physical — PA)** bằng cách tra **page table**. Thanh ghi **CR3** trỏ tới gốc bảng.

![[mmu-translation-flow.svg|1083]]

Ý nghĩa: kernel **không** sờ thẳng RAM — luôn qua một lớp dịch. Đây là lý do có HHDM (ý 5).

## 2. Trang (page) = 4 KiB — đơn vị nhỏ nhất

VA và PA đều chia thành các **trang 4 KiB (0x1000)**. Dịch địa chỉ = "trang ảo nào → khung vật lý (frame) nào", phần offset 12 bit thấp giữ nguyên.

```
VA = [ số trang ảo (52 bit cao) ][ offset 12 bit ]
                  │ dịch qua bảng        │ giữ nguyên
                  ▼                      ▼
PA = [ khung vật lý           ][ offset 12 bit ]
```

→ Vì đơn vị là 4 KiB nên `MAXPAGESIZE = 0x1000`, và `ALIGN(0x1000)` trong [[Step 00 - Boot & Limine]] chính là làm tròn lên biên trang.

## 3. Mỗi trang có quyền riêng: R / W / X (NX) → giải thích W\^X

Mỗi entry trong bảng mang các bit quyền:
- **P** (Present) — trang có map không
- **R/W** — cho ghi không (tắt = chỉ đọc)
- **U/S** — user hay supervisor
- **NX** (No-eXecute, bit 63) — cấm thực thi

Vì phân quyền chỉ ở **mức nguyên 1 trang**, hai vùng khác quyền không được chung trang → đó là lý do `link.ld` chèn `. = ALIGN(MAXPAGESIZE)` giữa các segment:

![[wx-page-split.svg]]

**W^X** = Write XOR eXecute: không trang nào vừa ghi-được vừa chạy-được.

## 4. x86_64 dùng 4 cấp bảng, CR3 trỏ gốc

VA 48 bit được cắt thành 4 chỉ số 9-bit + offset 12-bit, đi qua 4 tầng:

```
  VA: [ 9 bit ][ 9 bit ][ 9 bit ][ 9 bit ][ 12 bit offset ]
        PML4  →  PDPT  →   PD   →   PT   →  byte trong trang

  CR3 ──► PML4 ──► PDPT ──► PD ──► PT ──► Frame 4KiB
         (mỗi bảng 512 entry × 8 byte = đúng 1 trang)
```

Cần nhớ ở mức này: **"TỐI ĐA 4 tầng, CR3 trỏ gốc, mỗi bảng 512 entry"**. ("Tối đa" vì có thể dừng sớm bằng
hugepage — xem #4c.) Chi tiết duyệt bảng để dành tới step VMM.

### Vì sao mỗi bảng đúng 512 entry và vừa khít 1 trang 4 KiB?
**Cố ý thiết kế cho mỗi bảng = đúng 1 trang.** Mỗi entry là con trỏ 64-bit = **8 byte**. Một trang 4 KiB chứa được `4096 / 8 = 512` entry → cần **9 bit** để đánh số (2⁹ = 512). Đó chính là lý do VA cắt thành các nhóm **9-bit**:

```
1 trang = 4096 byte ÷ 8 byte/entry = 512 entry = 2^9  →  chỉ số mỗi tầng = 9 bit
```

Nhờ vậy bản thân page table cũng được cấp phát gọn bằng **một frame 4 KiB** (giống mọi trang khác) — PMM ở [[Step 04 - Physical Memory Manager]] chỉ việc cấp 1 frame là có 1 bảng.

## 4b. Ví dụ chạy số: VA → 4 bảng → PA

Lấy `VA = 0x0000008080604A30`. Chẻ 48 bit thành 4 chỉ số 9-bit + offset 12-bit:

```
VA = 0x0000_0080_8060_4A30

 [ 9 bit ][ 9 bit ][ 9 bit ][ 9 bit ][  12 bit  ]
   001      002      003      004      A30
  PML4i    PDPTi     PDi      PTi      offset
   = 1      = 2      = 3      = 4      = 0xA30
```

Đi qua 4 bảng — **bảng giữa chỉ "chỉ đường tới bảng sau", riêng PT cho ra khung vật lý thật + bit quyền**:

![[page-walk-4-level.svg]]

Ghép khung + offset (offset **chép thẳng** từ VA, không tra bảng):

```
 PA = (PFN << 12) | offset
    =  0x0FFB2000 | 0xA30
    =  0x0FFB2A30   ◄── địa chỉ vật lý tuyệt đối, hết dịch
```

Toàn cảnh — cái gì đổi, cái gì giữ nguyên:

![[va-split-to-pa.svg]]

- **36 bit trên của VA** (4 nhóm 9-bit) → qua 4 bảng → thành **PFN** của PA.
- **12 bit offset** → **không tra bảng, chép nguyên** sang PA.
- Một byte có thể có **nhiều VA** trỏ về **cùng 1 PA** (vd kernel: qua HHDM và qua kernel mapping — xem [[HHDM]]), nhưng PA luôn **duy nhất**.

## 4c. Hugepage: KHÔNG phải VA nào cũng đi đủ 4 tầng
> _"4-level" = **tối đa** 4 tầng, không phải luôn 4. Một entry có thể **dừng sớm** và phủ thẳng vùng lớn._

Mỗi entry ở PDPT/PD có bit **PS (Page Size)**. Bật `PS=1` → entry đó là **lá** (trỏ thẳng vùng nhớ), bỏ
các tầng dưới. Bỏ 1 tầng = offset dài thêm 9 bit = vùng phủ to lên **512 lần**:

```
dừng ở PT  (PS không có ở PT) : offset 12 bit → 2^12 = 4 KiB   (trang thường, 4 tầng)
dừng ở PD  (PS=1)             : offset 21 bit → 2^21 = 2 MiB   (hugepage 2M, 3 tầng — bỏ PT)
dừng ở PDPT(PS=1)             : offset 30 bit → 2^30 = 1 GiB   (hugepage 1G, 2 tầng — bỏ PD+PT)
```
Tức **1 entry "nhân" lên**: PD entry hugepage = 512 × 4 KiB = 2 MiB (gộp 512 trang con thành 1).

**Số tầng là per-NHÁNH, không per-bảng** — cùng 1 PML4 trộn được:
![[pml4-branches-huge-vs-4k.svg]]
MMU walk **từng VA độc lập**, gặp `PS=1` thì dừng. (Lưu ý: bản thân **bảng** PML4/PDPT/PD vẫn 4 KiB; "2 MiB"
là vùng *một entry* phủ, không phải cỡ bảng.)

**Lợi ích (vì sao HHDM dùng hugepage 2M)**: map 256 MB chỉ cần 128 entry PD (1 bảng) thay vì 65536 entry PT
(128 bảng) → ít RAM page table; walk 3 tầng + TLB phủ 2 MiB/entry → nhanh hơn. HHDM map "nguyên khối RAM"
không cần quyền mịn từng 4 KiB nên rất hợp. Kernel thì dùng 4 KiB để áp **W^X** riêng từng section (ý #3).
→ Đo thật ở [[Lab 0x03 - PMM & VMM]] (`paging_dump_walk`): HHDM dừng ở PD (2MB), kernel đủ 4 tầng.

## 5. HHDM & higher-half: kernel sống ở nửa cao địa chỉ ảo

Limine map sẵn **toàn bộ RAM vật lý** vào một vùng VA cố định ở **nửa cao** (higher half), gọi là **HHDM** (Higher Half Direct Map). Công thức:

```
VA = PA + hhdm_offset      (hhdm_offset lấy từ Limine, xem [[Step 02 - Bootloader Parser]])
```

Nhờ vậy kernel muốn đọc PA bất kỳ chỉ cần cộng offset → ra VA dùng được ngay, **không phải tự dựng page table** lúc đầu. `kernel_addr`, `memmap` trong Step 02 đều xoay quanh ý này.

```
  Không gian VA 64-bit:
  0x0000...          (nửa thấp = user, sau này)
  ───────────────────────────  ← canonical hole
  0xFFFF_8000_...   HHDM: PA + offset (toàn bộ RAM map ở đây)
  0xFFFF_FFFF_8000_0000  kernel image (.text/.rodata/.data)
```

---

## Móc nối vào cavOS

| Khái niệm paging | Gặp ở đâu trong cavOS |
|---|---|
| ALIGN biên trang, W^X | [[Step 00 - Boot & Limine]] — `link.ld` |
| HHDM offset, kernel_addr | [[Step 02 - Bootloader Parser]] |
| memmap (vùng RAM usable) | [[Step 02 - Bootloader Parser]] |
| Dựng page table thật, CR3 | Step VMM (sau) — lúc đó mới mở Intel SDM Vol.3 Ch.4 |

## Tài nguyên (ngắn, đúng việc)
- **OSDev Wiki** — *Paging* + *Setting Up Paging* (sát OS hobby x86_64 nhất).
- **OSTEP** — chương *Address Translation* & *Paging* (miễn phí, bottom-up).
- **Intel SDM Vol.3 Ch.4** — ĐỂ DÀNH, chỉ tra khi cần bit cụ thể.
