---
tags: [lab, hands-on, memory, paging]
status: done
---

# Lab 0x03 — PMM & VMM

> **Thực hành [[Step 04 - Physical Memory Manager]] + [[Step 05 - Virtual Memory & Paging]]** (gộp cụm — PMM
> và VMM phải đi chung). Tự dựng bitmap cấp frame, rồi tự WALK page table map VA→PA và chứng minh nó chạy.

**Vị trí:** `~/oskernel-lab/03-pmm-vmm/` · base revision **2**. **Chạy:** `make run`.

> [!note] Source viết lại & đã chạy lại (2026-09-03) — CÓ SỬA 1 BUG
> Source hiện tại viết lại từ note → `Labs/src/oskernel-lab/03-pmm-vmm/`, **đã boot QEMU thật lại**.
> Bản cũ đánh dấu **thiếu 1 frame** của bitmap: bitmap 8301 B tại `0x60000` trải 3 frame
> (`0x60000/0x61000/0x62000`) nhưng chỉ mark 2 → `pmm_alloc` phát ra chính `0x62000` **đang chứa 109 byte
> cuối của bitmap**. Không sập vì PML4 mới chỉ dùng entry 192/256/511 (offset từ 1536 byte trở đi) → may
> chứ không đúng. Bản mới làm tròn **LÊN**, đã xác nhận vá đúng ở mục "Chạy lại 2026-09-03" bên dưới.
> Số cũ + `outputs/lab-0x03-run.txt` giữ nguyên để đối chiếu; output mới ở
> `outputs/lab-0x03-run-2026-09-03.txt`.

## 📦 Cấu trúc module
```
io.h        — outb/inb (inline)
serial.{h,c}— UART 16550 (Step 01)
boot.{h,c}  — Limine requests + hhdm + P2V/PAGE + memset8 (Step 00/02)
pmm.{h,c}   — bitmap PMM + pmm_dump_map (Step 04)
paging.{h,c}— VMM/page table: paging_init/vmap/vresolve (Step 05)
kernel.c    — kmain: orchestrate + in kết quả
```
`SRCS := kernel.c boot.c serial.c pmm.c paging.c`. String output **tiếng Anh**; comment tiếng Việt.

## Làm gì
**PMM (Step 04):** bitmap 1 bit/frame 4 KiB từ memmap (`0xff`=used mặc định → mở USABLE → đóng non-USABLE),
`pmm_alloc/free`, truy cập bitmap qua HHDM. **VMM (Step 05):** tự dựng PML4 mới + `mov cr3`, `vmap()` walk 4
tầng cấp bảng con từ PMM, `vresolve()` dịch ngược VA→PA. Test: ghi qua VA mới, đọc lại qua HHDM của cùng PA.

## 🗺️ Bộ nhớ vật lý KHÔNG liền mạch (mấu chốt của PMM)
> _Đừng tưởng RAM là một dải liền từ 0. Không gian địa chỉ vật lý là bản đồ chung; RAM chỉ chiếm vài mảnh,
> xen kẽ là vùng cấm (BIOS, ACPI) và MMIO (thiết bị). Đây là lý do PMM phải đọc memmap._

Dump memmap thật (QEMU `-m 256M`, từ [[Lab 0x01 - Bootloader Parser]]):
```
 PA thấp                                                                    PA cao
 0x1000        0x100000              0xfd000000      0xfd00000000
 ┌──┬───────┬──┬───────────────┬──┬──────┬─────────┬────────────┬───────────────┐
 │BR│USABLE │R │   USABLE      │BR│KERNEL│FRAMEBUF │ (lỗ trống) │  RESERVED     │
 │  │ 252KB │  │  ~254 MiB     │  │ MODS │ (MMIO)  │            │  (MMIO cao)   │
 └──┴───────┴──┴───────────────┴──┴──────┴─────────┴────────────┴───────────────┘
   ▲ RAM thật ↑                          ▲ KHÔNG phải RAM ──────────────────────►
   (USABLE/BOOT_RECLAIM = cấp được)      (RESERVED/FRAMEBUFFER = thiết bị, cấm cấp)

 BR=BOOT_RECLAIM  R=RESERVED   ·   các con số PA KHÔNG liền nhau, có "lỗ"
```
3 điều rút ra:
- **RAM nằm rải rác**, không liền: có lỗ ở `0x9fc00` (BIOS), quanh `0xfd000000` (PCI hole), và một vùng
  RESERVED tít ở `0xfd00000000`.
- **Địa chỉ cao ≠ RAM**: `0xfd000000` (framebuffer) và `0xfd00000000` là **MMIO** — ghi vào = điều khiển
  thiết bị, không lưu dữ liệu. Xem [[Step 03 - Framebuffer & Console]], [[HHDM]].
- → Nếu tính tổng RAM bằng "địa chỉ cuối cao nhất" thì **sai** (phình theo lỗ MMIO). Phải **cộng dồn length
  các vùng RAM thật** — đó là lý do `mmTotal` (xem dưới).

## ⚙️ Tính kích thước bitmap — bám đúng cavOS (`mmTotal`)
cavOS `bootloader.c` tính tổng RAM = **cộng dồn `length` các vùng KHÁC `RESERVED`** (không lấy max-end):
```c
mmTotal = 0;
for (mỗi entry)
    if (entry->type != LIMINE_MEMMAP_RESERVED)   // RESERVED = MMIO/PCI hole → bỏ
        mmTotal += entry->length;                // cộng dồn RAM thật + reclaimable
pmm_blocks = DivRoundUp(mmTotal, 4096);          // số frame cần theo dõi
pmm_bytes  = DivRoundUp(pmm_blocks, 8);          // cỡ bitmap
```
Lab đã sửa đúng kiểu này (trước đó lab dùng "max-end" → bitmap phình 33 MB; giờ chỉ ~8 KB).

> 🛡️ **Guard biên `bm_mark`**: vòng "đóng non-USABLE" có chạm entry MMIO địa chỉ rất cao (`0xfd00000000`)
> → block index vượt xa `pmm_blocks`. Nếu không chặn sẽ **ghi tràn ngoài bitmap**. Lab thêm
> `if(first+i >= pmm_blocks) break;` — chỉ đánh dấu trong phạm vi bitmap.

## Kết quả thật (QEMU `-M q35 -m 256M`, 2026-06-01)
> 📄 **Output ĐẦY ĐỦ** (bảng `pmm_dump_map` 6 cột địa chỉ — PA/VA-HHDM/VA-kernel begin+end — rộng ~176 cột,
> không vừa markdown) lưu ở: **`outputs/lab-0x03-run.txt`** (cùng thư mục Labs). Dưới đây là phần rút gọn.

```
[pmm] total frames = 66405  bitmap bytes = 8301
[pmm] bitmap at PA = 0x60000  (block #96)  VA = 0xffff800000060000
[pmm] alloc #1 PA = 0x62000   alloc #2 = 0x63000   (giảm 2, free lại → OK)
[vmm] built new PML4 + mov cr3;  our pml4 (VA) = 0xffff800000062000
[vmm] map VA 0x600000000000 -> PA 0x63000
[vmm] tables for VA: PDPT=0x64000 (new) PD=0x65000 (new) PT=0x66000 (new)
[vmm] write via new VA, read via HHDM = 0xdeadbeefcafebabe  OK (same PA!)
[vmm] vresolve(VA) = 0x63000  OK
```

**Trích bảng `pmm_dump_map`** (bản đầy đủ 6 cột begin+end ở file .txt; đây bỏ cột end cho gọn). Mỗi **VÙNG**
memmap 1 dòng, kèm dòng con `>` cho từng **OBJECT** nằm trong vùng đó:

```
| PA begin   | VA-HHDM begin      | VA-kernel begin    | type     | alloc | stores       | size     |
| 0x00060000 | 0xffff800000060000 | -                  | USABLE   |   Y   | (region)     |   252 K  |
| 0x00060000 | 0xffff800000060000 | -                  |  (obj)   |   .   | > PMM bitmap |     8 K  | ← object con
| 0x00062000 | 0xffff800000062000 | -                  |  (obj)   |   .   | > PML4 (new) |     4 K  |   nằm CÙNG vùng
| 0x00064000 | 0xffff800000064000 | -                  |  (obj)   |   .   | > PDPT testVA|     4 K  |   USABLE 0x60000,
| 0x00065000 | 0xffff800000065000 | -                  |  (obj)   |   .   | > PD testVA  |     4 K  |   mỗi bảng đúng
| 0x00066000 | 0xffff800000066000 | -                  |  (obj)   |   .   | > PT testVA  |     4 K  |   1 frame 4 KiB
| 0x0ff53000 | 0xffff80000ff53000 | 0xffffffff80000000 | KERNEL   |   n   | (region)     |    16 K  | ← 2 dải VA, 1 PA = aliasing
| 0x0ff86000 | 0xffff80000ff86000 | -                  | BOOT_REC |   Y   | (region)     |   356 K  |
| 0x0ff9e000 | 0xffff80000ff9e000 | -                  |  (obj)   |   .   | > PML4 (old) |     4 K  | ← bảng Limine cũ
| 0xfd000000 | -                  | -                  | FRAMEBUF |   n   | framebuffer  |  4000 K  |
```
> Cột **size**: object page table đều `4 K` (= 1 frame, đúng "512 entry × 8 byte"); bitmap `8 K` (~3 frame).

> ⚠️ **PDPT/PD/PT trong bảng là `testVA` — của riêng VA test `0x600000000000`**, do lab `vmap` cấp. KHÔNG phải
> bảng con của HHDM/kernel! Bảng con HHDM/kernel do **Limine dựng**; `paging_init` copy 512 entry PML4 = copy
> **con trỏ** → PML4 (new) **dùng chung** các bảng con đó với PML4 (old) → chúng nằm trong vùng Limine
> (BOOT_REC), lab không cấp nên **không liệt kê**. Đó là lý do copy 512 entry là đủ để HHDM+kernel vẫn chạy
> sau `mov cr3`. (Bảng chỉ hiện những gì **lab tự cấp**: bitmap, PML4 new, 3 bảng testVA; cộng PML4 old đọc từ CR3.)

> 🔬 **Bảng con HHDM/kernel nằm PA nào?** `paging_dump_walk` walk thật 2 VA cho thấy:
> ```
> HHDM   VA=0xffff800000000000:  PML4[256] -> PDPT@0x0ff9a000 -> PD@0x0ff99000 -> 2MB hugepage
> kernel VA=0xffffffff80000000:  PML4[511] -> PDPT@0x0ff9d000 -> PD@0x0ff9c000 -> PT@0x0ff9b000
> ```
> - Tất cả bảng con (`0x0ff99000`–`0x0ff9d000`) **nằm trong vùng BOOT_REC `0x0ff86000–0x0ffdf000`** — cùng chỗ
>   PML4 old `0x0ff9e000`. → **Limine gom cả cây page table của nó vào BOOT_REC**; PML4 new mượn qua con trỏ.
> - **HHDM dùng 2 MiB hugepage** (`PS=1` ở PD) → chỉ 3 tầng (PML4→PDPT→PD), KHÔNG có PT → 1 entry PD phủ 2 MiB,
>   tốn rất ít bảng cho cả 256 MB. **Kernel dùng trang 4 KiB** (đủ 4 tầng) để áp quyền mịn (W^X) cho .text/.data.
> - `PML4[256]` (HHDM, nửa cao `0xffff8000…`) vs `PML4[511]` (kernel, top `0xffffffff80…`) — đúng chỉ số đã tính.

> **Dòng `(region)`** = 1 vùng memmap; **dòng `> ...`** = 1 object **nhỏ hơn** vùng cha. Thấy rõ **bitmap +
> cả 4 tầng page table (PML4/PDPT/PD/PT) nằm liền nhau** trong USABLE `0x60000` (PMM cấp tuần tự); `PML4 (old)`
> của Limine ở vùng BOOT_REC khác. Vùng **KERNEL** không có dòng con vì nó **chính là** kernel image (cùng
> begin/end), và có **cả VA-HHDM lẫn VA-kernel** → cùng PA, 2 VA = **aliasing**.

> 🔑 **Đọc bảng:**
> - **VA HHDM (RAM only)**: chỉ vùng RAM mới hiện VA (`PA + hhdmOffset`); `RESERVED`/`FRAMEBUF`/MMIO để `-`
>   → nhìn cột là biết ngay đâu là RAM. (Lưu ý: HHDM của Limine thực ra map cả dải thấp, nhưng bảng cố ý chỉ
>   hiện VA cho vùng RAM để khỏi nhầm "có VA = là RAM".)
> - **VA (kernel map)**: chỉ **đúng 1 dòng** `KERNEL` (`0xffffffff80000000`) — kernel image map riêng. Frame
>   kernel vì thế có **2 VA** (HHDM + kernel map) → **aliasing** ([[HHDM]] "dùng VA nào").
> - **alloc** `Y/n` = cấp phát được hay cấm; **bitmap** đúng 1 dòng `Y` (`0x60000`, "quyển sổ").

> 🕳️ **`address gap (not backed by RAM/device)` = đoạn địa chỉ KHÔNG có chip RAM lẫn thiết bị nào nối tới.**
> Không gian địa chỉ vật lý là dãy số 0→2⁴⁸, nhưng máy chỉ "đấu dây" RAM vào vài đoạn + để vài đoạn cho MMIO
> (thiết bị). Khoảng **giữa** các vùng mà memmap không liệt kê gì = **đất trống đã đánh số nhưng chưa xây**:
> ghi vào → mất hút, đọc ra → rác (`0xff`) hoặc lỗi bus. Khác với `RESERVED`/`FRAMEBUF` (vẫn là *device* —
> có thiết bị phía sau): gap thì **không cả RAM lẫn device**. **Không dùng được** vì *địa chỉ tồn tại ≠ có
> phần cứng phía sau*.
> → PMM cố ý **không cấp** chúng: `memset 0xff` (đóng tất) rồi chỉ mở vùng `USABLE` → frame trong gap luôn
> = used, `pmm_alloc` không bao giờ phát ra. (Vì sao máy để trống nhiều: xem §"Bộ nhớ vật lý không liền mạch".)

> 🧭 **Cột pgtbl (gốc page table — PML4 — nằm vùng nào)**: `old` ở `0x0ff86000` = **PML4 cũ của Limine**
> (CR3 lúc boot, nằm trong vùng BOOT_REC — Limine để cấu trúc của nó ở đây). `new` ở `0x60000` = **PML4 mới
> lab tự cấp** (frame đầu sau bitmap, `pmm_alloc` lấy ra). → Thấy tận mắt: bảng cũ trong vùng Limine, bảng mới
> trong RAM lab xin từ PMM → đúng chuyện "tự dựng bảng riêng + `mov cr3`" ([[Step 05 - Virtual Memory & Paging]]).

> 📊 **Đối chiếu số**: bitmap ở `0x60000` chiếm ~3 frame (8301 B) → `alloc #1 = 0x62000` rơi ngay sau sổ.
> `free -2` rồi `free after free` về đúng số cũ → cấp/thu hồi đối xứng. VMM: ghi qua VA mới `0x600000000000`,
> đọc qua HHDM của cùng PA `0x63000` → khớp `0xdeadbeefcafebabe` (chứng minh aliasing); `vresolve` walk ngược ra đúng PA.

> 🌲 **Cây 4 tầng nằm đâu** (dòng `[vmm] tables for VA`): map VA `0x600000000000` lần đầu vào nhánh trống →
> `vmap` cấp **lazy** cả 3 bảng trung gian, đều `(new)`. Chúng nằm **sát nhau ngay sau PML4** (PMM cấp tuần tự):
> ```
> PML4 = 0x62000   (paging_init)
> PDPT = 0x64000   (new) ┐
> PD   = 0x65000   (new) ├ 3 bảng trung gian, lazy trong vmap
> PT   = 0x66000   (new) ┘  → entry lá PT trỏ frame dữ liệu 0x63000
> ```
> → Toàn bộ page table 4 tầng **đều là frame RAM USABLE** (cột pgtbl chỉ tô gốc PML4 vì lab chỉ lưu PA gốc;
> bảng con in qua dòng log này). Map VA thứ 2 *gần đó* sẽ thấy `(reuse)` thay vì `(new)` — tái dùng bảng cũ.

> 📐 **Sơ đồ "PA ←→ VA" trực quan** (mũi tên, aliasing): xem [[Lab 0x01 - Bootloader Parser]] — bảng
> `pmm_dump_map` ở trên đã thể hiện đủ 3 cột PA/VA-HHDM/VA-kernel cho mọi vùng.

## Chạy lại 2026-09-03 (source dựng lại, đã vá bug bitmap)

> _Cùng một cấu hình QEMU nhưng Limine xếp bộ nhớ khác lần trước, nên mọi địa chỉ đều dời. Cái **không**
> dời mới là bài học: tổng số frame, cỡ bitmap, và mọi quan hệ giữa các con số._
> Output đầy đủ: `outputs/lab-0x03-run-2026-09-03.txt`.

```
[pmm] total frames = 66405  bitmap bytes = 8301        ← Y HỆT lần cũ
[pmm] bitmap at PA = 0x0000000000053000  (block #83)   ← dời (cũ: 0x60000, block #96)
[pmm] free frames  = 65181
[pmm] alloc #1 PA  = 0x0000000000056000                ← BUG ĐÃ VÁ (xem dưới)
[pmm] alloc #2 PA  = 0x0000000000057000
[pmm] free -2 = 65179  OK      /  free after free = 65181  OK (back to start)

[vmm] our pml4 (VA) = 0xffff800000056000      PML4 old = 0x0ff88000
[walk] HHDM   PML4[0x100] -> PDPT@0x0ff84000 -> PD@0x0ff83000 -> 2MB hugepage
[walk] kernel PML4[0x1ff] -> PDPT@0x0ff87000[0x1fe] -> PD@0x0ff86000 -> PT@0x0ff85000
[vmm] map VA 0x600000000000 -> PA 0x57000
[vmm] tables: PDPT=0x58000 (new) PD=0x59000 (new) PT=0x5a000 (new)
[vmm] write via new VA, read via HHDM = 0xdeadbeefcafebabe  OK (same PA!)
[vmm] vresolve(VA) = 0x57000  OK
```

**Bug đã vá — kiểm bằng số:** bitmap `0x53000` → `0x5506d`, tức chiếm frame `0x53/0x54/0x55` (**3 frame**).
`alloc #1` ra `0x56000` = frame ngay **sau** frame cuối của bitmap. Bản cũ sẽ ra `0x55000` và giẫm lên
đuôi bitmap. Đối chiếu tiếp: USABLE có `76 + 65077 + 31 = 65184` frame, trừ 3 frame bitmap = **65181**,
đúng bằng `free frames` in ra → sổ sách khớp tuyệt đối.

**Cái gì cố định, cái gì đổi:**

| Đại lượng | Đổi? | Vì sao |
| --- | --- | --- |
| `total frames = 66405`, `bitmap bytes = 8301` | không | phụ thuộc **tổng RAM** (`mmTotal`), mà QEMU vẫn cấp 256 MiB |
| `hhdm offset = 0xffff800000000000` | không | hằng số của 4-level paging |
| PA của bitmap / PML4 / các bảng | đổi hết | Limine xếp vùng khác đi mỗi lần boot |
| Bảng con HHDM/kernel nằm trong BOOT_REC | không | Limine luôn gom cây page table của nó vào BOOT_REC |
| HHDM = 2 MiB hugepage (3 tầng), kernel = 4 KiB (4 tầng) | không | thiết kế của Limine, không phải ngẫu nhiên |

> Lần này bảng con của HHDM/kernel nằm `0x0ff83000`–`0x0ff87000`, cùng vùng BOOT_REC với PML4 old
> `0x0ff88000` — **đúng y kết luận rút ra từ lần chạy cũ** dù số đã khác hoàn toàn. Đó là dấu hiệu bài học
> đúng chứ không phải trùng hợp.

## 🔑 Điểm học chính

### 1. PMM tự host bitmap, đóng hết → mở USABLE → đóng non-USABLE
Bitmap đặt ngay trong vùng USABLE đầu tiên (qua HHDM). `memset 0xff` (mọi frame used) → `bm_mark(...,0)` mở
các vùng USABLE → `bm_mark(...,1)` đóng lại non-USABLE → tự mark chính bitmap. Đúng trình tự cavOS `pmm.c`.

> 🔍 **Bitmap nằm ở đâu** (in ra serial): `PA = 0x60000` (vùng USABLE đầu, khớp memmap), `block #96`
> (`0x60000/4096` — chính frame mà bitmap tự đánh dấu used), `VA = 0xffff800000060000` (= PA + hhdmOffset,
> [[HHDM]]). Bitmap chiếm ~3 frame (8301 B) → `0x60000..0x62000` → **alloc đầu rơi `0x62000`**, ngay sau sổ.
> Đây là "con gà–quả trứng": allocator đầu tiên phải tự cấp chỗ cho chính mình.

### 2. Tự dựng PML4 mới — để có "mov cr3 THẬT"
```c
uint64_t *new_pml4 = P2V(pmm_alloc());
for(int i=0;i<512;i++) new_pml4[i] = limine_pml4[i];   // copy mapping kernel + HHDM
asm volatile("mov %0,%%cr3"::"r"(new_pa));             // ← mov cr3 thật
```
Cấp 1 frame làm PML4 mới, **copy 512 entry** từ bảng Limine (giữ mapping kernel + HHDM), rồi `mov cr3` sang
bảng của ta → có **address space riêng**, sửa bảng thoải mái.

> ⚖️ **So với cavOS:** [[Step 05 - Virtual Memory & Paging]] cho thấy cavOS *tái dùng thẳng* bảng Limine
> (`globalPagedir = P2V(cr3)`), không tự dựng lúc boot. Lab tự dựng để **minh hoạ `mov cr3`** — cũng là thứ
> cavOS làm về sau khi tạo page directory mỗi task ([[Step 11 - Multitasking & Scheduler]]). Khác ở thời điểm.

### 3. `vmap` lazy 4 tầng + `invlpg`
Tầng nào chưa PRESENT → cấp frame mới từ **PMM** làm bảng con (đúng mắt xích Step 04→05). Map xong gọi
`invlpg` xoá TLB cho VA đó. `vresolve` walk ngược để xác nhận VA→PA khớp.

### 4. Aliasing 2 VA → 1 PA (chứng minh)
Ghi `0xdeadbeefcafebabe` qua **VA mới** `0x600000000000` (map → PA `0x63000`), đọc qua **VA HHDM**
`P2V(0x63000)` → cùng giá trị. Hai VA khác nhau, một frame → đúng [[HHDM]] (aliasing).

## ✅ Tự kiểm tra
1. Đổi cách tính `mmTotal` về "max-end" (không lọc RESERVED) → bitmap to bao nhiêu? Vì sao? (lỗ MMIO).
2. Bỏ guard `if(first+i >= pmm_blocks) break;` → đóng non-USABLE entry `0xfd00000000` sẽ ra sao? (ghi tràn).
3. Remap 1 VA sang PA khác mà QUÊN `invlpg` → đọc ra giá trị cũ hay mới? (vai trò TLB).
4. Bỏ copy 512 entry (PML4 mới để trắng) rồi `mov cr3` → chuyện gì xảy ra? (mất mapping kernel → sập).

## Liên hệ
- Lý thuyết: [[Step 04 - Physical Memory Manager]], [[Step 05 - Virtual Memory & Paging]] · nền [[Paging]], [[HHDM]].
- Build/os.img: [[Build & Debug Cheatsheet]].
