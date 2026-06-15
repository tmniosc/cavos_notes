---
tags: [step, cpu]
status: done
---
# Step 06 — GDT & TSS

**Files:** `cpu/gdt.c`, `include/gdt.h` · **Hàm:** `initiateGDT()` · **Spec:** [[Intel SDM]] Vol.3 (segmentation, TSS)

## 🎯 Mục đích
**Khai báo các "hạng" thực thi (ring 0 kernel / ring 3 user) và chỗ CPU lấy stack khi đổi ring.** Long mode
gần như bỏ segmentation, nhưng CPU **vẫn bắt buộc** có GDT để biết selector code/data + mức đặc quyền (CPL).
TSS cung cấp `RSP0` (stack kernel khi user→kernel qua interrupt/syscall) và IST (stack riêng cho fault hiểm).
Là tiền đề cho [[Step 08 - IDT & Interrupts]] và [[Step 15 - Fast Syscalls]].

### 🧒 Nói thật đơn giản
CPU x86 có một danh sách gọi là **GDT** (Global Descriptor Table) — mỗi dòng mô tả "một loại vùng code/dữ
liệu và ai được chạy nó" (kernel hay user). Ở 64-bit, phần "vùng nằm đâu, to bao nhiêu" gần như **bị bỏ qua**
(mọi segment phẳng, phủ cả bộ nhớ), nhưng CPU **vẫn đòi có bảng này** để biết: dòng này là *code hay data*,
*ring 0 (kernel) hay ring 3 (user)*. Giống thẻ ra vào: không quan trọng "phòng nào" nữa, chỉ còn quan trọng
"cấp bậc người cầm thẻ".

**TSS** (Task State Segment) thời 64-bit chỉ còn 1 việc chính: giữ sẵn **địa chỉ stack của kernel** (`RSP0`).
Khi chương trình user (ring 3) bị ngắt/gọi syscall → CPU nhảy vào kernel (ring 0) → nó cần một stack sạch của
kernel, và lấy địa chỉ đó từ TSS. (Chi tiết bit access/flags là phần kỹ thuật bên dưới.)

Trong `_start()`: `initiateGDT()` chạy sau VMM, trước GDT-phụ-thuộc như [[Step 08 - IDT & Interrupts]].

## 6.1 Cấu trúc GDT của cavOS (`gdt.h`)
> _Bảng gồm 11 "descriptor" (mỗi cái 8 byte) + 1 ô TSS (16 byte). Mỗi descriptor = 1 loại segment._

```c
typedef struct GDTEntries {
  GDTEntry descriptors[11];   // các segment code/data (kernel + user, 16/32/64-bit)
  TSSEntry tss;               // ô mô tả TSS (16 byte vì cần địa chỉ 64-bit)
} __attribute__((packed)) GDTEntries;
```
Một `GDTEntry` (8 byte): `limit`, `base_*`, `access`, `granularity`. **64-bit bỏ qua base/limit** — chỉ
`access` + 1 bit trong `granularity` (cờ Long) còn ý nghĩa.

## 6.2 Các descriptor — offset & ý nghĩa (`initiateGDT`)
> _Mỗi dòng là 1 "thẻ" loại segment. cavOS khai cả 16/32-bit (di sản) lẫn 64-bit (thực dùng). Selector =
> offset byte của descriptor trong bảng._

| offset | descriptor         | access (bin) | dùng                                 |
| ------ | ------------------ | ------------ | ------------------------------------ |
| 0      | null               | 0            | bắt buộc (selector 0 = vô hiệu)      |
| 8      | kernel code 16     | `10011010`   | di sản, không dùng ở long mode       |
| 16     | kernel data 16     | `10010010`   | di sản                               |
| 24     | kernel code 32     | `10011010`   | di sản (boot 32-bit)                 |
| 32     | kernel data 32     | `10010010`   | di sản                               |
| **40** | **kernel code 64** | `10011010`   | **CS ring 0** (`GDT_KERNEL_CODE`)    |
| **48** | **kernel data 64** | `10010010`   | **DS/SS ring 0** (`GDT_KERNEL_DATA`) |
| 56,64  | (SYSENTER, 0)      | —            | để trống                             |
| 72     | user data 64       | `11110010`   | **ring 3 data** (`GDT_USER_DATA`)    |
| 80     | user code 64       | `11111010`   | **ring 3 code** (`GDT_USER_CODE`)    |
| 88     | TSS                | —            | trỏ tới `TSSPtr`                     |

Giải mã byte `access` (bit quan trọng):
```
bit7 P=1 (present)  bit6-5 DPL (00=ring0, 11=ring3)  bit4 S=1 (code/data)
bit3 Exec (1=code,0=data)  bit1 RW
→ 10011010 = present, ring0, code, readable   (kernel code)
→ 11111010 = present, ring3, code, readable   (user code — DPL=11)
→ 11110010 = present, ring3, data, writable   (user data)
```

> ⚠️ Chú ý thứ tự "lạ": user **data ở 72**, user **code ở 80** (code khai sau data trong `initiateGDT`).
> Quan trọng cho `sysret` ([[Step 15 - Fast Syscalls]]) vốn đòi user code/data ở offset liền kề theo thứ tự cố định.

## 6.3 `granularity` & cờ Long (bit L)
> _Ở long mode, thứ duy nhất của granularity còn nghĩa là bit "đây là segment 64-bit"._

- Kernel/user **code 64**: `granularity = 0b00100000` → bit **L=1** (Long mode 64-bit).
- Data 64 & null: granularity 0 (data không cần bit L).
- Code/data 32 (di sản): `0b11001111` (G=1 4KiB, D/B=1 32-bit) — chỉ để boot 32-bit cũ, long mode không dùng.

## 6.4 `gdt_reload()` — nạp GDT + "nhảy" để CS có hiệu lực
> _Ghi địa chỉ bảng vào CPU (`lgdt`) chưa đủ — phải làm CPU NẠP LẠI thanh ghi đoạn (CS/DS/...) thì selector
> mới có hiệu lực. CS đặc biệt: chỉ đổi được bằng một cú "nhảy xa"._

```asm
lgdt gdtr                 ; nạp địa chỉ + giới hạn GDT
push $0x28                ; 0x28 = 40 = kernel code 64 (CS mới)
lea 1f(%rip), %rax ; push %rax
lretq                     ; "far return" → nạp CS = 0x28, nhảy tới nhãn 1
1: mov $0x30, %eax        ; 0x30 = 48 = kernel data 64
   mov %eax → ds/es/fs/gs/ss   ; nạp các thanh ghi đoạn data
```

- **`lretq` (far return)** là mẹo kinh điển để **đổi CS** — không thể `mov` thẳng vào CS.
- DS/ES/FS/GS/SS nạp `0x30` (kernel data). Ở long mode hầu hết bị bỏ qua nhưng vẫn set cho sạch.

## 6.5 TSS — chỉ để giữ `RSP0` (`gdt_load_tss`)
> _TSS 64-bit gần như rỗng nghĩa, chỉ field `RSP0` (+ IST) là sống còn: stack kernel dùng khi user→kernel._

```c
typedef struct TSSPtr {
  uint32_t unused0; uint64_t rsp0, rsp1, rsp2;   // rsp0 = stack ring0 (quan trọng nhất)
  uint64_t unused1, ist1..ist7;                  // IST = stack riêng cho fault hiểm
  ...
} TSSPtr;
```
- `gdt_load_tss`: điền địa chỉ `tss` vào descriptor TSS (offset 88), rồi `ltr 0x58` (load task register).
- `flags1 = 0b10001001`: P=1, type=0b1001 (available 64-bit TSS).
- **RSP0 sẽ được điền sau** (khi có scheduler/task — [[Step 11 - Multitasking & Scheduler]]): mỗi lần chuyển
  task, cập nhật `tss.rsp0` = stack kernel của task đó. Lúc này `memset(tss,0)` → rsp0=0, chưa dùng tới.

## ✅ Câu hỏi mở (đã trả lời)
- **Long mode bỏ base/limit nhưng vẫn cần descriptor — vì sao?** → CPU vẫn cần biết **CPL (ring 0/3)** + loại
  (code/data) + cờ Long. Selector CS/SS vẫn phải trỏ vào descriptor hợp lệ; `iretq`/`sysret` cần các segment
  user/kernel ở đúng offset.
- **IST dùng cho fault nào?** → fault mà stack hiện tại có thể hỏng/không tin được: **double fault, NMI,
  machine check**. IST cho CPU một stack **đã biết tốt** để xử lý (dùng ở [[Step 08 - IDT & Interrupts]]).

## Liên hệ
- 📌 **Nền tảng spec chuyên sâu về GDT** (descriptor từng bit, selector, CPL/DPL/RPL, 32 vs 64-bit): [[GDT]].
- IDT/interrupt dùng selector kernel code + IST/RSP0: [[Step 08 - IDT & Interrupts]].
- syscall/sysret dùng user/kernel segment + RSP0: [[Step 15 - Fast Syscalls]].
- RSP0 cập nhật mỗi lần đổi task: [[Step 11 - Multitasking & Scheduler]].

## Ánh xạ spec
- [[Intel SDM]] Vol.3 §3 (segmentation, descriptor), §7 (TSS), §5 (privilege/CPL/DPL), bit L (IA-32e code segment).
