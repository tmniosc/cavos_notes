---
tags: [concept, cpu, segmentation, history]
---
# x86 Segmentation & 3 chế độ (real / protected / long)

> **"x86 segment" là cách CPU x86 chỉ tay vào bộ nhớ qua các thanh ghi đoạn (CS/DS/SS...).** Khái niệm này
> tiến hoá qua 3 chế độ — **real → protected → long** — nhưng cốt lõi vẫn là một. Note này gỡ rối cảm giác
> "nhiều thứ quá". Chi tiết bảng descriptor: [[GDT]]; chế độ 64-bit: [[Long Mode]].

### 🧒 Nói thật đơn giản
> *Đừng hoảng — đây **không phải 4-5 khái niệm rời**, mà là **MỘT ý duy nhất già đi qua 3 thời kỳ**.*
>
> *Ngày xưa CPU 16-bit chỉ "với" tới 64KB bộ nhớ (thanh ghi quá ngắn). Mẹo: tách địa chỉ làm 2 mảnh —
> một mảnh chỉ "khúc nào" (gọi là **segment**), một mảnh chỉ "trong khúc đó vị trí nào" (offset). **Segment
> = khúc bộ nhớ CPU đang trỏ tới.** CPU giữ vài "ngón tay" trỏ khúc: CS trỏ khúc code, DS trỏ khúc data, SS
> trỏ khúc stack.*
>
> *Rồi qua từng đời CPU, ý nghĩa của "khúc" này đổi: thời đầu (real mode) nó chỉ là số nhân thô để với xa
> hơn; thời giữa (protected mode) nó thành tấm vé trỏ vào bảng [[GDT]] có kèm bảo vệ (ai được vào, vùng tới
> đâu); thời nay (long mode 64-bit) nó **gần như tắt** — mọi khúc phủ cả bộ nhớ, chỉ còn giữ "kernel hay
> user". Cái lo việc phân vùng bộ nhớ thật bây giờ là [[Paging]], không phải segment nữa.*
>
> *Tin tốt cho cavOS: máy đã chạy ở long mode, segment gần như vô dụng → bạn chỉ cần nhớ **một câu** về nó
> (xem mục 🎯 cuối), khỏi học sâu real/protected mode.*

## 1. "x86 segment" là gì — gốc rễ
> *Vấn đề năm 1978: thanh ghi 16-bit chỉ đếm tới 64KB, nhưng muốn xài 1MB RAM. Làm sao với xa hơn tầm tay?*

Giải pháp: **tách địa chỉ làm 2 phần**.
```
địa chỉ = segment : offset
          (khúc)    (lệch trong khúc)
```
CPU giữ các **thanh ghi đoạn (segment register)**, mỗi cái trỏ một "khúc":

| Thanh ghi | Trỏ khúc gì |
| --------- | ----------- |
| **CS** | **C**ode đang chạy |
| **DS** | **D**ata |
| **SS** | **S**tack |
| ES / FS / GS | data phụ (FS/GS đặc biệt ở long mode — xem [[GDT]] §10) |

Mọi truy cập bộ nhớ luôn **ngầm đi qua một segment register**. Đó là "x86 segment" — không phải một vùng
RAM cụ thể, mà là **cơ chế CPU dùng để định địa chỉ**. Cơ chế này *chưa bao giờ biến mất*, chỉ đổi ý nghĩa.

> 🔑 Phân biệt với **ELF segment**: trùng tên nhưng khác hẳn. ELF segment (`.text`/`.data`) là khối để nạp,
> quyền của nó do **[[Paging|page table]]** enforce — KHÔNG phải x86 segment / GDT. Xem [[Step 17 - Userspace & ELF Loader]].

## 2. Ba chế độ = ba thời kỳ của cùng một segment
> *Mỗi đời CPU thêm tính năng nhưng giữ tương thích ngược → trông như nhiều thứ, thật ra là một thứ chồng lớp.*

| Mode | CPU / năm | Segment register chứa gì? | Tính địa chỉ ra sao? | Bảo vệ? |
| ---- | --------- | ------------------------- | -------------------- | ------- |
| **Real mode** (16-bit) | 8086, 1978 | một **con số thô** | `addr = segment × 16 + offset` → với tới 1MB | ❌ không, ai cũng đụng mọi nơi |
| **Protected mode** (32-bit) | 80286/386 | một **selector** (số ghế trỏ [[GDT]]) | CPU tra GDT → `base + offset`, kiểm `limit` + ring | ✅ có (base/limit + DPL) |
| **Long mode** (64-bit) | x86-64 | vẫn **selector** trỏ GDT | `base=0, limit=∞` → **phẳng, gần như tắt** | quyền giờ do [[Paging]] lo |

### Mạch tiến hoá (nhớ đúng cái này là đủ)
![[segmentation-three-modes.svg]]

### Vì sao boot phải đi qua cả 3?
CPU x86 **luôn khởi động ở real mode** (tương thích 8086 từ 1978), rồi bootloader leo dần: real → bật A20,
dựng GDT → protected → bật PAE + paging + `EFER.LME` → long. **[[Limine Protocol|Limine]] làm hết khúc này
giùm cavOS** → khi vào `_start` CPU đã ở long mode (xem [[Boot Flow (_start)]], [[Long Mode]]). Đó là lý do
bạn **không cần** học sâu asm chuyển mode.

## 3. Vì sao long mode "khai tử" segment?
> *Segment và paging cùng làm một việc — phân vùng & bảo vệ bộ nhớ — nên giữ cả hai là thừa. Paging thắng vì
> nó mịn hơn (từng trang 4KiB) và hợp với địa chỉ ảo per-tiến-trình.*

- Segment chỉ chia được vài "khúc" lớn theo base/limit; **[[Paging]]** chia tới từng trang 4KiB, mỗi trang
  có quyền R/W/NX/U-S riêng → linh hoạt hơn hẳn.
- Vậy nên ở long mode: **base bị ép = 0, limit bỏ qua** cho CS/DS/SS/ES. Segment thành "phẳng" (phủ cả không
  gian địa chỉ tuyến tính). Thứ duy nhất còn sống: **CPL (ring 0/3) trong CS** + cờ code/data.
- (Ngoại lệ: **FS/GS base** vẫn dùng được, lấy từ MSR — cho per-CPU/per-thread data. Xem [[GDT]] §10.)

## 🎯 Phần bạn THỰC SỰ cần cho cavOS (đọc cái này là đủ)
cavOS chạy **long mode**, Limine đã đưa CPU qua real→protected→long sẵn. Nên:

- **Real mode / protected mode**: chỉ cần biết **tồn tại trong lịch sử** + boot đi qua chúng. **Không học sâu.**
- **Long mode segment** — nhớ đúng **một câu**:
  > *Segment gần như tắt; chỉ còn dùng để khai **ring 0 (kernel) / ring 3 (user)** + **code/data** thô, qua [[GDT]].*
- Công sức nên đổ vào **[[Paging]]** (cái thay segment để quản bộ nhớ thật) — bạn đã học ở [[Step 05 - Virtual Memory & Paging]] rồi. 👍

## 🔗 Liên hệ
- Bảng descriptor & cách dùng segment ở 64-bit (từng bit): [[GDT]].
- Chế độ 64-bit & các bit/MSR chuyển mode: [[Long Mode]].
- Cái thay thế segment để phân vùng/bảo vệ: [[Paging]].
- cavOS dựng GDT cụ thể: [[Step 06 - GDT & TSS]].

## 📖 Ánh xạ spec
- [[Intel SDM]] Vol.3A §3 (segmentation, real & protected mode), §9 (mode switching), §5.x (IA-32e flat segmentation).
- AMD64 APM Vol.2 §1–2 (system overview, long mode) — dễ đọc, mô tả long mode rõ.
