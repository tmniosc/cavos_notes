---
tags: [step, boot]
status: done
---

# Step 00 — Boot entry & Limine protocol

**Files:** `link.ld`, `entry/kernel.c`, `entry/bootloader.c`, `include/limine.h`
**Khái niệm:** [[Limine Protocol]], [[Request-Response Mechanism]], [[Higher-Half Kernel]], [[HHDM]], [[Long Mode]], [[KASLR & PIE Kernel]]
**🔬 Thực hành:** [[Lab 0x00 - Hello Serial]] — tự viết lại phần boot+serial từ số 0 (gộp với Step 01).

## 🎯 Mục đích
**Đưa CPU từ lúc bật nguồn tới hàm C đầu tiên của kernel (`_start`)** — và hiểu kernel được nạp vào bộ
nhớ ra sao. Thay vì tự viết bootloader (chuyển real→protected→long mode, bật paging...), cavOS để **Limine**
lo hết rồi giao lại CPU đã ở [[Long Mode]] + paging + [[HHDM]]. Step này trả lời: *kernel layout thế nào
(`link.ld`), Limine nạp ELF ra sao, và kernel "đặt hàng" thông tin phần cứng bằng cách nào* ([[Limine Protocol]]).

### 🧒 Nói thật đơn giản
Bật máy lên, CPU chưa biết kernel của bạn là gì. Cần ai đó: (1) đọc file kernel từ đĩa, (2) đặt CPU vào
chế độ 64-bit + bật bộ nhớ ảo, (3) nhảy vào hàm đầu tiên của kernel. Việc đó **rất khó và nhiều bước asm**.
→ cavOS thuê sẵn **Limine** (một bootloader có sẵn) làm hết, rồi "giao chìa khoá" cho kernel ở hàm `_start`.

Cả bài này xoay quanh 3 câu hỏi:
- **Kernel xếp vào bộ nhớ thế nào?** File `link.ld` là "bản thiết kế": nói kernel nằm ở địa chỉ nào, phần
  code/dữ liệu xếp ra sao, mỗi phần được quyền gì (đọc/ghi/chạy). Mỗi phần nằm trang riêng để gán quyền khác nhau.
- **Limine nạp kernel ra sao?** Kernel là file kiểu **ELF** (định dạng chương trình chuẩn). Limine biết đọc
  định dạng đó: chép từng phần vào RAM, dọn sạch vùng biến chưa khởi tạo, rồi nhảy vào `_start`.
- **Kernel xin thông tin phần cứng kiểu gì?** Kernel để sẵn các "phiếu yêu cầu" trong file; Limine thấy thì
  điền câu trả lời vào (RAM bao nhiêu, màn hình ở đâu...). Bài [[Step 02 - Bootloader Parser]] sẽ đọc các câu trả lời này.

Tóm lại: bạn **không phải tự viết bootloader** — chỉ cần hiểu kernel được bày vào bộ nhớ thế nào và Limine
trao lại những gì. (Chi tiết ELF header, PHDRS, relocation... là phần kỹ thuật, đọc bên dưới khi cần.)

## 0.1 `link.ld` — kernel trông thế nào trong bộ nhớ
> _"Bản thiết kế" xếp kernel vào bộ nhớ: nằm địa chỉ nào, các phần (code/dữ liệu) theo thứ tự gì, phần nào
> được quyền gì._

```ld
OUTPUT_FORMAT(elf64-x86-64)   ; kernel là ELF64
ENTRY(_start)                 ; điểm vào
. = 0xffffffff80000000;       ; nạp ở higher-half (−2GiB)
```
- Địa chỉ nửa cao `0xffffffff80000000`: xem [[Higher-Half Kernel]] (sign-extend + `-mcmodel=kernel`).
- `PHDRS`: mỗi `PT_LOAD` = 1 segment với quyền MMU riêng (text R+X, rodata R, data R+W) → **W^X**.
- `PT_DYNAMIC` + relocation → kernel **PIE**, Limine relocate ([[KASLR & PIE Kernel]]).
- Mỗi section căn `MAXPAGESIZE` để nằm trên ranh giới trang riêng (gán quyền MMU khác nhau) — xem
  [[Paging]] ý #3 (W^X) và `. = ALIGN(CONSTANT(MAXPAGESIZE));`.

### 0.1.2 PHDRS: tên là NHÃN, quyền đến từ section flags
> _Mấy chữ `text`/`rodata`/`data` chỉ là tên gọi cho vui — không phải phép thuật. Quyền (đọc/ghi/chạy) thật
> ra được suy ra từ loại của từng phần, không phải từ cái tên._

`text`/`rodata`/`data` trong khối `PHDRS` **không phải tên ma thuật** — chúng là **nhãn tự đặt**
(đổi thành `foo/bar/baz` vẫn chạy y hệt). Thứ ld thật sự hiểu chỉ là loại **`PT_LOAD`** (= "nạp vào RAM").

Vì script **không ghi `FLAGS(...)`**, ld **tự suy quyền segment = OR flag của các section bên trong**:

```
tên section (.text…)  → as/gcc gán flag section → ld OR thành flag segment → ELF p_flags → page table (W^X)

:text     .text                AX (alloc+exec)  →  PF_R|PF_X  → R+X
:rodata   .rodata, .limine_*   A  (alloc)       →  PF_R       → R
:data     .data, .bss          WA (alloc+write) →  PF_R|PF_W  → R+W
```

Quy tắc ld: allocatable → luôn **R**; có section ghi-được → thêm **W**; có section exec → thêm **X**.
Flag section lại do **tên section chuẩn** quyết định (`.text`=AX, `.rodata`=A, `.data`/`.bss`=WA).

> 💡 Muốn tường minh (nhiều OS hobby làm): `text PT_LOAD FLAGS(5);` `rodata FLAGS(4);` `data FLAGS(6);`
> — bit **1=Execute, 2=Write, 4=Read** (5=R+X, 4=R, 6=R+W). Khi đó ld dùng số bạn ghi thay vì tự suy.

→ Tóm: ld **không** đọc chữ "text" để biết R+X; quyền chảy từ **tên section → flag section → flag segment**.

## 0.1.1 Limine nạp ELF vào bộ nhớ thế nào (boot-time loader)
> _Limine biết đọc file kernel (định dạng ELF) rồi bày từng phần vào RAM đúng chỗ — giống mở hộp đồ rồi xếp
> đồ vào kệ theo bản thiết kế — xong mới "bấm nút" chạy kernel._

Limine **có sẵn ELF64 loader**. Việc parse + nạp xảy ra ở **boot time** (Limine còn nắm quyền, *trước*
khi nhảy vào kernel) — không phải "runtime" của kernel. Loader đọc theo trình tự:

1. **ELF header** — check magic `7f 45 4c 46`, `Class ELF64`, `Type EXEC`, lấy **`e_entry`** (= `ENTRY(...)`).
2. **Program Headers** (KHÔNG phải section headers) — chỉ quan tâm các segment `PT_LOAD`. Mỗi `PT_LOAD`:
   cấp RAM vật lý → copy `p_filesz` byte từ `p_offset` → **zero-fill** phần `p_memsz - p_filesz` (cách `.bss`
   được dọn về 0) → **map `p_vaddr` → physical** với quyền lấy từ `p_flags` (R/W/X).
3. **PT_DYNAMIC** — xử lý relocation (kernel PIE) → đặt kernel ở base ngẫu nhiên ([[KASLR & PIE Kernel]]).
4. Chuẩn bị môi trường ([[Long Mode]], [[HHDM]], stack) rồi **nhảy vào `_start`**.

## 0.2 Limine protocol — request/response
> _Cách kernel "hỏi" và Limine "đáp": kernel để sẵn phiếu yêu cầu trong file, Limine tìm thấy thì điền câu
> trả lời vào._

Kernel khai báo **request** (struct có magic `.id`), Limine quét binary, điền **response**.
Cơ chế: [[Request-Response Mechanism]]. Danh mục request: [[Limine Protocol]].
Bước *đọc* response thật nằm ở [[Step 02 - Bootloader Parser]].

## 0.3 `_start()` — điểm vào
> _Hàm C đầu tiên của kernel — nơi Limine "thả" CPU vào. Từ đây kernel tự lo mọi thứ._

Xem bản đồ thứ tự khởi tạo: [[Boot Flow (_start)]]. Step 00 mới chỉ tới lúc CPU nhảy vào `_start`;
việc đầu tiên `_start` làm là bật serial ([[Step 01 - Serial UART]]).

## Ánh xạ spec
- ELF64: System V ABI / ELF spec (program headers, `PT_LOAD`, `PT_DYNAMIC`).
- Long mode, paging: [[Intel SDM]] Vol.3 (để dành chi tiết).
- Limine boot protocol: [[Danh mục tài liệu]].
