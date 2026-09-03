---
tags: [step, memory, paging]
status: done
---

# Step 05 — Virtual Memory & Paging (VMM)

**Files:** `memory/paging.c`, `memory/vmm.c`, `include/paging.h`, `include/vmm.h`
**Đầu vào:** frame từ [[Step 04 - Physical Memory Manager]], `hhdmOffset` từ [[Step 02 - Bootloader Parser]]
**Nền tảng:** [[Paging]] (đọc kỹ #4 4 cấp bảng + ví dụ 4b), [[Intel SDM]] Vol.3 Ch.4

## 🎯 Mục đích
**Quản lý không gian địa chỉ ẢO: map VA→PA tùy ý, đặt quyền cho từng trang, và cấp vùng nhớ ảo.** PMM
([[Step 04 - Physical Memory Manager]]) cho frame thô; step này quyết định **frame đó xuất hiện ở địa chỉ ảo
nào, quyền gì**. Cung cấp `VirtualMap()` (sửa page table) làm nền cho: heap/malloc kernel, mmap, và mỗi process
một address space riêng ([[Step 11 - Multitasking & Scheduler]], [[Step 17 - Userspace & ELF Loader]]).
PMM lo *có/không* — VMM lo *đặt ở đâu*.

### 🧒 Nói thật đơn giản
PMM ([[Step 04 - Physical Memory Manager]]) cho bạn **một ô RAM trống**, nhưng ô đó có **địa chỉ vật lý**
(số thật trên thanh RAM). Vấn đề: code kernel lại chạy bằng **địa chỉ ảo**. Cần một "bảng phiên dịch"
(*page table*) nói "địa chỉ ảo A → ô vật lý B, được đọc/ghi/chạy hay không". Step 05 là người **viết bảng đó**.

Ví von xuyên suốt: PMM cấp cho bạn **căn phòng** (ô RAM), VMM **dán số nhà** (địa chỉ ảo) lên cửa để tìm tới.
Cả bài gồm:
- **`vmap` — dán số nhà**: nối một địa chỉ ảo với một ô RAM. Nếu "con đường" dẫn tới đó chưa có thì dựng thêm
  (xin ô trống từ PMM làm bảng phụ). Dán xong phải báo CPU "đừng dùng bản ghi nhớ cũ" (xoá *TLB*).
- **`VirtualToPhysical` — tra ngược**: từ số nhà tìm lại căn phòng thật.
- **Mỗi chương trình một bộ số nhà riêng**: sau này mỗi tiến trình có bảng riêng → cùng một địa chỉ ảo ở 2
  chương trình trỏ tới 2 ô RAM khác nhau ([[Step 11 - Multitasking & Scheduler]]).
- **Cùng phòng, nhiều số nhà** (*aliasing*): một ô RAM có thể có nhiều địa chỉ ảo cùng trỏ tới (xem [[HHDM]]).

Điều bất ngờ khi đọc code: cavOS **không tự dựng bảng mới** lúc khởi động — nó **dùng lại bảng Limine** đã
lập sẵn, chỉ thêm/sửa vài dòng. (Định dạng bit của bảng, đổi CR3... là phần kỹ thuật bên dưới.)

Trong `_start()`: `initiateVMM()` (56) → ... → `initiatePaging()` (63). VMM chạy **trước** paging vì
allocator ảo của nó chỉ dựa trên HHDM (đã có sẵn), chưa cần đụng page table.

## ⚠️ Đính chính quan trọng: cavOS KHÔNG tự dựng page table mới
> _Ai cũng tưởng kernel sẽ tự lập "bảng phiên dịch" của riêng mình. Thực ra cavOS lười hơn: nó dùng lại
> bảng Limine đã làm sẵn, chỉ ghi thêm vài dòng khi cần._

Trái với hình dung "tự `mov cr3` lần đầu", `initiatePaging()` **giữ nguyên page table Limine đã dựng**,
chỉ **đọc CR3 hiện tại** và lưu con trỏ (qua HHDM) để sửa về sau:
```c
void initiatePaging() {
  uint64_t pdPhys;
  asm volatile("movq %%cr3,%0" : "=r"(pdPhys));   // lấy CR3 Limine đang dùng (PA của PML4)
  globalPagedir = (uint64_t *)(pdPhys + bootloader.hhdmOffset); // PA→VA để sửa bảng
}
```
> Phần "tự build layout y hệt link.ld rồi ChangePageDirectory" **đã bị comment** trong source (tác giả
> ghi *"send help"* 😅). cavOS **tái dùng bảng Limine** — vốn đã map sẵn kernel higher-half + HHDM +
> framebuffer. Kernel chỉ **thêm/sửa entry** trên bảng đó, không thay CR3 lúc boot.
> → Đúng tinh thần [[HHDM]] mục "CR3 vs HHDM": CR3 luôn chạy (của Limine), ta chỉ chỉnh nội dung bảng.
> → `mov cr3` thật chỉ xảy ra sau này khi **đổi address space giữa các task** ([[Step 11 - Multitasking & Scheduler]]).

## 5.1 Định dạng entry & macro tách VA (`paging.h`)
> _Mỗi dòng trong "bảng phiên dịch" vừa ghi địa chỉ ô RAM, vừa ghi quyền (đọc/ghi/chạy). Và để tra bảng,
> ta phải cắt địa chỉ ảo thành 4 mẩu — mỗi mẩu chỉ ra một tầng bảng._

Bit cờ trong mỗi entry (khớp [[Intel SDM]] Vol.3 §4.5, bảng PTE):
```c
PF_PRESENT (1<<0)  PF_RW (1<<1)  PF_USER (1<<2)  PF_PWT (1<<3) PF_PCD (1<<4)
PF_ACCESS (1<<5)   PF_DIRTY (1<<6) PF_PS (1<<7)  PF_GLOBAL (1<<8)
PF_CACHE_WC = PF_PAT|PF_PWT   // write-combining cho VRAM
```
> ⚠️ **cavOS KHÔNG dùng NX (bit 63)** — `paging.h` không có `PF_NX`. Nghĩa là **W^X thực tế chưa được
> enforce** ở runtime (dù `link.ld` tách trang sẵn — xem [[Step 00 - Boot & Limine]] §0.1.2). Tách trang
> W^X chỉ có hiệu lực nếu loader/kernel set NX; ở đây Limine có thể set theo `p_flags` lúc nạp, nhưng các
> entry kernel **tự map** (VirtualMap) đều `PRESENT|RW|USER` — khá lỏng. Đây là chỗ có thể siết sau.

Tách VA 48-bit thành 4 chỉ số 9-bit (đúng ví dụ 4b note Paging):
```c
PML4E(a) = (a >> 39) & 0x1ff      PDPTE(a) = (a >> 30) & 0x1ff
PDE(a)   = (a >> 21) & 0x1ff      PTE(a)   = (a >> 12) & 0x1ff
PTE_GET_ADDR(v) = v & 0x000ffffffffff000   // lấy PFN, bỏ 12 bit cờ + bit cao
AMD64_MM_STRIPSX(a) = a & 0xFFFFFFFFFFFF    // bỏ sign-extend 16 bit cao trước khi index
```

## 5.2 `VirtualMap()` — map 1 trang VA→PA (trái tim Step 05)
> _Đây là hàm "dán số nhà": cho một địa chỉ ảo và một ô RAM, nó nối hai cái lại. Đi qua 4 tầng bảng; tầng
> nào thiếu thì dựng thêm. Dán xong báo CPU bỏ trí nhớ cũ._

```c
VirtualMap(virt, phys, flags):
  tách 4 index từ virt;
  // đi xuống từng tầng, TẦNG NÀO CHƯA CÓ thì cấp frame mới làm bảng con:
  if (!(pml4[i] & PRESENT)) pml4[i] = PagingPhysAllocate() | PRESENT|RW|USER;
  pdp = PTE_GET_ADDR(pml4[i]) + HHDMoffset;     // ← đọc bảng con qua HHDM
  ... lặp cho pdp→pd→pt ...
  pt[pt_index] = P_PHYS_ADDR(phys) | PRESENT | flags;  // ghi entry lá
  invalidate(virt);                                     // invlpg: xoá TLB cho VA này
```
3 điểm cốt lõi:
- **Lazy table allocation**: bảng con (PDPT/PD/PT) chỉ cấp khi cần, lấy frame từ **PMM** ([[Step 04 - Physical Memory Manager]]).
  `PagingPhysAllocate()` = `PhysicalAllocate(1)` rồi `memset 0` qua HHDM (bảng mới phải sạch).
- **Mọi truy cập bảng đều qua HHDM** (`PTE_GET_ADDR(...) + HHDMoffset`): entry chứa **PA** của bảng con,
  cộng offset để ra VA đọc/ghi. Đây là lý do PMM/VMM/paging **bắt buộc cần HHDM** ([[HHDM]] "dùng VA nào").
- **`invlpg`** sau khi sửa: CPU cache bản dịch trong **TLB**; sửa bảng xong phải vô hiệu hoá kẻo dùng bản cũ.

## 5.3 `VirtualToPhysical()` — dịch ngược (walk bảng bằng phần mềm)
> _Chiều ngược lại: cho số nhà (địa chỉ ảo), tìm lại căn phòng thật (địa chỉ vật lý). Chính là việc CPU làm
> tự động, nhưng đây viết lại bằng C để kernel tự tra khi cần._

```c
// Tối ưu: nếu VA nằm trong vùng HHDM → trả thẳng virt - HHDMoffset (khỏi walk)
if (virt >= HHDMoffset && virt <= HHDMoffset + max(mmTotal, 4GiB))
    return virt - HHDMoffset;
// ngược lại: tự đi PML4→PDPT→PD→PT, mỗi tầng check PRESENT, lấy PFN + offset 12-bit
```
→ Bản mô phỏng đúng việc MMU làm bằng phần cứng (ví dụ 4b note Paging), nhưng bằng C.

## 5.4 VMM — allocator vùng địa chỉ ảo (`vmm.c`)
> _Muốn xin RAM để dùng trong kernel? Hàm này xin ô trống từ PMM rồi trả về địa chỉ kernel dùng được ngay.
> Hiện rất đơn giản vì nhờ HHDM, mọi RAM đã có sẵn "số nhà"._

```c
void *VirtualAllocate(int pages) {
  size_t phys = PhysicalAllocate(pages);   // xin frame vật lý từ PMM
  return (void *)(phys + bootloader.hhdmOffset);  // trả VA HHDM dùng ngay
}
```
- Hiện tại VMM **rất mỏng**: cấp RAM kernel = lấy frame PMM + trả VA HHDM (đã map sẵn → không cần `VirtualMap`).
- `initiateVMM()` dựng một `DS_Bitmap virtual` (cùng cấu trúc bitmap [[Step 04 - Physical Memory Manager]]) cho
  vùng VA động, đặt `mem_start` ngay dưới vùng HHDM (`hhdmOffset - mmTotal - 1GiB`). Bitmap này host trong RAM
  qua `VirtualAllocate`.
- `VirtualFree` = `VirtualToPhysical` rồi `PhysicalFree`.

## 5.5 Page directory cho task (dùng ở Step 11/17)
> _Mỗi chương trình cần một "bộ số nhà" riêng để không giẫm lên nhau. Phần này tạo bảng riêng cho từng
> task — nhưng vẫn chép chung phần kernel để chương trình nào cũng gọi được dịch vụ kernel._

- `PageDirectoryAllocate()`: cấp PML4 mới, **copy 512 entry** từ bảng kernel → mọi task **chia sẻ mapping
  kernel** (nửa cao), chỉ khác phần user (nửa thấp). Đúng lý do higher-half ([[Higher-Half Kernel]]).
- `ChangePageDirectoryUnsafe()`: **đây mới là `mov cr3` thật** — đổi address space khi chuyển task.
- `PageDirectoryUserDuplicate()` (fork): copy các entry **PF_USER**, frame chia sẻ nếu `PF_SHARED`, còn lại
  copy-toàn-bộ-trang. Dùng cho [[Step 17 - Userspace & ELF Loader]].
- `PageDirectoryFree()`: chỉ giải phóng entry **PF_USER** (không đụng mapping kernel chia sẻ).

## ✅ Câu hỏi mở (đã trả lời)
- **Tách kernel/user address space?** → mọi task copy 512 entry PML4 của kernel (nửa cao chung), chỉ
  thêm/bớt phần user nửa thấp; đổi task = `mov cr3` (`ChangePageDirectoryUnsafe`).
- **TLB invalidation?** → `invlpg` mỗi lần `VirtualMap`; reload CR3 khi đổi cả address space.
- **Kernel có tự dựng bảng không?** → KHÔNG lúc boot; tái dùng bảng Limine, chỉ sửa entry. Bảng riêng chỉ
  sinh ra cho từng task về sau.

## Liên hệ
- Frame để làm bảng + map: [[Step 04 - Physical Memory Manager]].
- `malloc`/heap kernel xây trên `VirtualAllocate` (`memory/malloc.c`).
- Đổi CR3 thật + per-task pagedir: [[Step 11 - Multitasking & Scheduler]], fork ELF: [[Step 17 - Userspace & ELF Loader]].

## Ánh xạ spec
- [[Intel SDM]] Vol.3 Ch.4 — Paging: format PML4E/PDPTE/PDE/PTE, bit P/RW/US/PS/PAT/NX, CR3, INVLPG.
- AMD64 canonical sign-extension (bit 47): `AMD64_MM_STRIPSX`/`ADDRSX`.
