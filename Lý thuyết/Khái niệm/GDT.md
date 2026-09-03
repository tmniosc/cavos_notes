---
tags: [concept, cpu, segmentation]
---

# GDT — Global Descriptor Table

> **Bảng mô tả đoạn (segment) toàn cục** của CPU x86. Mỗi dòng (descriptor) mô tả một "vùng" bộ nhớ:
> nằm đâu, to bao nhiêu, ai được chạy (ring 0/3), là code hay data. CPU **bắt buộc** có bảng này ở
> protected mode & long mode. Note này giảng GDT **theo spec x86** (góc nhìn nền tảng); cách cavOS dùng
> cụ thể xem [[Step 06 - GDT & TSS]].

### 🧒 Nói thật đơn giản
> *Tưởng tượng RAM là một toà nhà lớn. Ngày xưa CPU chia toà nhà thành nhiều "phòng" (segment) và mỗi
> phòng có một **tấm thẻ mô tả**: phòng này từ tầng nào tới tầng nào, ai được vào (sếp hay nhân viên),
> được làm gì (đọc/ghi/chạy). **GDT chính là tập tấm thẻ đó** — một danh sách, mỗi dòng = 1 tấm thẻ
> (gọi là descriptor). CPU giữ trong tay một "số ghế" (selector) để biết đang dùng tấm thẻ nào.*
>
> *Thời 32-bit, mấy tấm thẻ này nói rõ "phòng ở đâu, to bao nhiêu" — phân vùng bộ nhớ thật sự. Sang
> 64-bit (long mode), người ta thấy cách chia phòng này lỗi thời (đã có [[Paging]] lo việc đó tốt hơn),
> nên CPU **gần như bỏ qua phần "phòng ở đâu, to bao nhiêu"** — mọi phòng coi như phủ cả toà nhà (phẳng).
> Nhưng CPU **vẫn đòi có bảng thẻ**, vì nó cần đọc 2 thứ còn ý nghĩa: tấm thẻ này là **code hay data**, và
> **ai được cầm** (kernel ring 0 hay user ring 3). Giống thẻ ra vào công ty: không còn quan trọng "phòng
> nào" nữa, chỉ còn quan trọng "cấp bậc người cầm thẻ".*
>
> *Ngoài ra trong bảng còn vài tấm thẻ "hệ thống" đặc biệt, quan trọng nhất là [[Step 06 - GDT & TSS|TSS]]
> — chỗ CPU cất sẵn địa chỉ stack của kernel để dùng khi user gọi vào kernel.*

---

## 0. GDT thao tác ở **tầng địa chỉ nào**? (chống hiểu lầm cốt lõi)
> *Câu hỏi hay gặp: "GDT là cho RAM hay cho toàn bộ physical memory?" → **Không phải cái nào cả.** GDT
> thao tác trên **không gian địa chỉ tuyến tính (linear/virtual)** — một khái niệm logic, KHÔNG phải bản
> đồ bộ nhớ vật lý. Nó không biết và không quan tâm máy có bao nhiêu RAM hay RAM nằm đâu.*

Một địa chỉ trên x86 đi qua **2 tầng dịch** trước khi chạm RAM thật:
![[addr-translation-stages.svg]]

- **GDT ở tầng segmentation** → `base`/`limit` của descriptor mô tả một "cửa sổ" trong **không gian địa chỉ
  tuyến tính** (ảo), KHÔNG phải trong RAM vật lý.
- Thứ ánh xạ địa chỉ ảo → physical là **[[Paging]]** (page table, `cr3`); thứ biết "RAM nằm đâu, bao nhiêu,
  vùng nào reserved" là **memmap từ bootloader** ([[Step 02 - Bootloader Parser]]) + [[Step 04 - Physical Memory Manager|PMM]].
  **Không phải GDT.**

> ⚠️ Khi note này nói segment long mode "phẳng, **phủ cả bộ nhớ**", nghĩa là `base=0, limit=∞` → segment
> trùm **toàn bộ không gian địa chỉ tuyến tính** (mọi offset ảo đều hợp lệ) — **không** phải "phủ toàn bộ
> RAM vật lý". Hai thứ khác tầng, đừng lẫn.

| Bảng / cấu trúc | Trả lời câu hỏi gì | Tầng |
| --------------- | ------------------ | ---- |
| **GDT** | "Địa chỉ *ảo* này là code/data? Ai (ring 0/3) được đụng?" | logic (segmentation) |
| **Page table** | "Địa chỉ *ảo* này ánh xạ tới *frame vật lý* nào?" | virtual → physical (paging) |
| **memmap (bootloader)** | "Vùng *vật lý* nào là RAM dùng được / reserved?" | mô tả physical thật |

→ Gọn: **GDT là bảng *phân loại & cấp quyền* trên địa chỉ ảo, KHÔNG phải bảng "chỗ nào có RAM".** Ở 32-bit
nó còn dùng `limit` chặn "đoạn ảo này tới đâu", nhưng vẫn là không gian tuyến tính; rồi paging mới quyết
địa chỉ đó có thật trong RAM không. Ở long mode segmentation gần như tắt → mọi việc "ở đâu trong RAM" do
paging lo trọn.

---

## 1. Vì sao có segmentation? (lịch sử)
> *Segmentation ra đời để CPU 16-bit "với" tới nhiều RAM hơn khả năng của một thanh ghi; rồi tiến hoá
> thành cơ chế bảo vệ; rồi gần như biến mất khi paging lên ngôi.* (Bản gỡ rối đầy đủ + 3 chế độ:
> [[x86 Segmentation]].)

| Đời CPU              | Vai trò segmentation                                                                 |
| -------------------- | ------------------------------------------------------------------------------------ |
| **8086** (1978, 16-bit) | Thanh ghi 16-bit chỉ địa chỉ được 64KiB. Mẹo: `địa chỉ = segment×16 + offset` → với tới 1MiB. Segment lúc này chỉ là **số nhân**, không có bảo vệ. |
| **80286** (1982) | Thêm **protected mode**: segment không còn là số nhân thô mà là **selector** trỏ vào một bảng descriptor (GDT/LDT). Bắt đầu có base/limit/quyền truy cập. |
| **80386** (1985, 32-bit) | Descriptor 32-bit đầy đủ (base 32-bit, limit 20-bit + granularity). Đồng thời thêm **[[Paging]]** → từ đây segmentation và paging cùng tồn tại. |
| **x86-64 / long mode** | Paging thắng tuyệt đối. Segmentation **gần như bị vô hiệu**: base=0, limit=∞ cho hầu hết segment. Chỉ còn giữ lại phần "loại + đặc quyền" → xem [[Long Mode]]. |

> 💡 Ý lớn: **GDT không biến mất, nó teo lại.** Ở long mode nó từ "công cụ phân vùng bộ nhớ" thành "tấm
> bảng khai báo ring 0/ring 3 + code/data" — tối thiểu để CPU chịu chạy.

## 2. GDT đứng ở đâu giữa các bảng descriptor?
> *x86 có 3 bảng descriptor; đừng lẫn lộn chúng.*

- **GDT** (Global Descriptor Table) — bảng **toàn cục**, dùng chung mọi tác vụ. Chứa segment code/data,
  TSS, LDT-descriptor. Đây là note này.
- **LDT** (Local Descriptor Table) — bảng **cục bộ** cho từng task (di sản, gần như không OS hiện đại nào
  dùng). Bản thân LDT được trỏ tới bởi 1 descriptor *nằm trong* GDT.
- **[[Step 08 - IDT & Interrupts|IDT]]** (Interrupt Descriptor Table) — bảng **trình xử lý ngắt**. Khác hẳn
  GDT về nội dung (gate descriptor), nhưng các gate này lại **tham chiếu selector trong GDT** (kernel code).

![[gdt-table-layout.svg]]

## 3. GDTR — thanh ghi trỏ tới bảng
> *CPU không giữ cả bảng trong người; nó chỉ giữ một con trỏ + độ dài, gọi là GDTR. Nạp bằng lệnh `lgdt`.*

GDTR là thanh ghi **10 byte** (ở long mode): 2 byte `limit` + 8 byte `base` — vẽ ở sơ đồ mục 2 bên trên.

- **limit** = *kích thước bảng tính theo byte − 1*. Vì mỗi descriptor 8 byte, bảng N entry → `limit = N*8 − 1`.
- **base** = địa chỉ bắt đầu bảng (32-bit ở protected mode, 64-bit ở long mode).
- Nạp: `lgdt [gdtr]`. Đọc lại: `sgdt`.
- ⚠️ Nạp `lgdt` **chưa làm gì ngay** — selector trong các thanh ghi đoạn vẫn là giá trị cũ. Phải **reload
  segment register** (đặc biệt CS) thì descriptor mới có hiệu lực → xem mục 8.

## 4. Định dạng một descriptor (8 byte) — bóc từng bit
> *Đây là trái tim của GDT. Một descriptor đoạn code/data dài đúng 8 byte, nhồi nhét base/limit/quyền vào
> các vị trí "rải rác" vì lý do tương thích ngược với 80286.*

Sơ đồ 8 byte (đọc theo little-endian, byte 0 ở địa chỉ thấp):

![[gdt-descriptor-8byte.svg]]

| Trường         | Rộng   | Ý nghĩa                                                             |
| -------------- | ------ | ------------------------------------------------------------------ |
| **limit** 0:15 | 16 bit | 16 bit thấp của giới hạn đoạn                                       |
| **base** 0:15  | 16 bit | 16 bit thấp của địa chỉ nền                                         |
| **base** 16:23 | 8 bit  | byte tiếp của base                                                  |
| **access**     | 8 bit  | byte quyền truy cập (mục 5)                                         |
| **limit** 16:19 + **flags** | 4+4 bit | nibble thấp = 4 bit cao của limit; nibble cao = flags (mục 6) |
| **base** 24:31 | 8 bit  | byte cao nhất của base (đủ base 32-bit)                             |

→ Ghép lại: **base 32-bit** (4 mảnh: 0:15, 16:23, 24:31) + **limit 20-bit** (2 mảnh: 0:15, 16:19).

> 🧩 Vì sao base/limit bị xé lẻ? Tương thích nhị phân với descriptor 80286 cũ (chỉ có base 24-bit, limit
> 16-bit nằm ở 4 byte đầu). 80386 nhét thêm các bit vào byte 6-7 mà không phá layout cũ.

### Granularity & limit thực tế
- Bit **G** (granularity, trong flags): G=0 → limit tính theo **byte** (tối đa 1MiB). G=1 → limit nhân
  4KiB → đoạn tới **4GiB** (`limit=0xFFFFF` × 4KiB).
- Đó là cách 32-bit phủ toàn bộ 4GiB bằng một segment "phẳng" (base=0, limit=0xFFFFF, G=1).

## 5. Byte `access` — bảo vệ & loại đoạn
> *Một byte này quyết định: đoạn có tồn tại không, ai được dùng (ring), là hệ thống hay code/data, code
> hay data, đọc/ghi được không. Đây là byte hay tra cứu nhất.*

![[gdt-access-byte.svg]]

| Bit       | Tên | Ý nghĩa                                                                                   |
| --------- | --- | ----------------------------------------------------------------------------------------- |
| **7 P**   | Present  | 1 = descriptor hợp lệ (đoạn có mặt). P=0 → dùng tới sẽ #NP fault.                     |
| **6-5 DPL** | Descriptor Privilege Level | Ring yêu cầu để dùng đoạn: `00`=ring0 (kernel), `11`=ring3 (user). |
| **4 S**   | System   | 1 = đoạn **code/data thường**; 0 = đoạn **hệ thống** (TSS, LDT, gate) → layout khác (mục 7). |
| **3 E**   | Executable | 1 = **code**; 0 = **data**.                                                            |
| **2 DC**  | Direction/Conforming | *Data:* hướng lớn lên (0) / xuống (1, dùng cho stack). *Code:* conforming (1 = ring thấp hơn được gọi). |
| **1 RW**  | Read/Write | *Code:* 1 = đọc được (code không bao giờ ghi). *Data:* 1 = ghi được.                  |
| **0 A**   | Accessed | CPU tự set khi đoạn được dùng. OS thường khai 0.                                         |

**Ví dụ giải mã** (khớp với cavOS, xem [[Step 06 - GDT & TSS]]):
```
10011010 = P=1, DPL=00, S=1, E=1, DC=0, RW=1, A=0  → kernel code (ring0, code, readable)
10010010 = P=1, DPL=00, S=1, E=0, DC=0, RW=1, A=0  → kernel data (ring0, data, writable)
11111010 = P=1, DPL=11, S=1, E=1, DC=0, RW=1, A=0  → user   code (ring3, code, readable)
11110010 = P=1, DPL=11, S=1, E=0, DC=0, RW=1, A=0  → user   data (ring3, data, writable)
```

## 6. Nibble `flags` — kích thước & long mode
> *4 bit cao của byte 6 quyết định đoạn là 16/32/64-bit và đơn vị limit.*

![[gdt-flags-nibble.svg]]

| Bit     | Tên | Ý nghĩa                                                                          |
| ------- | --- | -------------------------------------------------------------------------------- |
| **G**   | Granularity | limit × 1 byte (0) hay × 4KiB (1).                                        |
| **D/B** | Default size | 0 = đoạn 16-bit, 1 = đoạn 32-bit. **Ở code 64-bit phải = 0** (vì L=1).   |
| **L**   | Long | 1 = **đoạn code 64-bit** (chỉ có nghĩa cho code segment ở long mode).            |
| **AVL** | Available | bit tự do cho OS dùng.                                                       |

> ⚠️ Ràng buộc long mode: với code segment 64-bit phải **L=1 và D/B=0**. `L=1, D/B=1` là **bất hợp lệ**
> (dành cho mở rộng tương lai). Đây là lý do descriptor code 64-bit của cavOS có `granularity = 0b00100000`
> (chỉ bit L) chứ không phải `0b11001111` như code 32-bit.

## 7. Descriptor hệ thống (S=0) — TSS, LDT dài 16 byte
> *Khi S=0, descriptor không còn là code/data mà là "đoạn hệ thống". Ở long mode, TSS/LDT cần địa chỉ
> 64-bit nên descriptor phình lên **16 byte** (gấp đôi).*

- Khi **S=0**, 4 bit `E/DC/RW/A` của byte access đổi nghĩa thành **Type 4-bit**:
  - `0x9` = TSS 64-bit *available*, `0xB` = TSS 64-bit *busy*, `0x2` = LDT, `0xC/0xE/0xF` = call/interrupt/trap gate.
- **TSS descriptor (long mode) = 16 byte**: 8 byte đầu giống layout thường, 8 byte sau chứa **base 32:63**
  (nửa cao địa chỉ 64-bit) + reserved. Vì vậy trong cavOS bảng khai `descriptors[11]` (8 byte mỗi cái) **+
  1 ô `tss` riêng 16 byte** → xem [[Step 06 - GDT & TSS]] §6.1.
- **TSS dùng để làm gì ở 64-bit?** Hầu như rỗng nghĩa, chỉ giữ `RSP0` (stack kernel khi user→kernel) và
  **IST** (Interrupt Stack Table — stack riêng cho fault hiểm: double fault, NMI, machine check). Chi tiết
  ở Step 06 §6.5 và [[Step 08 - IDT & Interrupts]].
- Nạp TSS: `ltr <selector>` (load task register), trỏ vào TSS descriptor trong GDT.

## 8. Segment selector — "số ghế" trỏ vào bảng
> *Thanh ghi đoạn (CS, DS, SS...) không chứa descriptor, chúng chứa một **selector 16-bit** = "lấy dòng
> thứ mấy trong bảng, ở ring nào". CPU dùng selector để tra ra descriptor.*

![[segment-selector-bits.svg]]

| Trường    | Ý nghĩa                                                                        |
| --------- | ------------------------------------------------------------------------------ |
| **Index** | Thứ tự descriptor trong bảng (13 bit → tối đa 8192 entry).                      |
| **TI**    | Table Indicator: 0 = tra GDT, 1 = tra LDT.                                      |
| **RPL**   | Requested Privilege Level (mục 9).                                              |

🔑 **Mẹo quan trọng:** vì mỗi descriptor 8 byte và Index nằm từ bit 3, **giá trị selector = offset byte của
descriptor trong bảng**. Ví dụ descriptor thứ 5 (offset 40 byte) → selector kernel code = `0x28` = 40 (với
RPL=0, TI=0). Đây là vì sao cavOS dùng `push $0x28` để nạp CS = kernel code 64. Selector **0x00 = null** →
luôn vô hiệu (truy cập sẽ #GP), nên dòng đầu GDT bắt buộc là null descriptor.

## 9. CPL / DPL / RPL — cơ chế bảo vệ theo ring
> *Ba chữ "PL" dễ lẫn. Chúng cùng phối hợp để CPU quyết định: "lệnh đang chạy này có được đụng vào đoạn
> kia không?". Quy tắc lõi: **số càng nhỏ càng nhiều quyền** (ring 0 = toàn quyền).*

| Viết tắt | Là gì | Ở đâu |
| -------- | ----- | ----- |
| **CPL** | *Current* Privilege Level — ring hiện tại CPU đang chạy. | 2 bit thấp của **CS** (và SS). |
| **DPL** | *Descriptor* Privilege Level — ring tối thiểu cần để dùng đoạn. | byte access của descriptor. |
| **RPL** | *Requested* Privilege Level — ring "tự khai" của selector khi truy cập. | 2 bit thấp của selector. |

- **x86 có 4 ring (0–3)** nhưng thực tế OS chỉ dùng **ring 0 (kernel)** và **ring 3 (user)**; ring 1/2 bỏ.
- Quy tắc thô khi nạp data segment: yêu cầu **max(CPL, RPL) ≤ DPL** thì mới cho. RPL tồn tại để kernel
  không bị lừa truy cập hộ user vào vùng cấm (tự hạ quyền bằng cách set RPL=3).
- Chuyển ring (user↔kernel) **không** làm bằng `mov` thường — phải qua **interrupt/`iretq`**, **`syscall`/
  `sysret`** ([[Step 15 - Fast Syscalls]]), hay call gate. Khi user→kernel, CPU lấy stack ring 0 từ
  **`RSP0` trong TSS**.

> 💡 Ở long mode, **CPL = 2 bit thấp của CS** là cách duy nhất CPU biết "đang là kernel hay user" — và đó
> chính là lý do GDT *không thể bỏ hẳn* dù base/limit vô dụng.

## 10. Segment register & phần "ẩn" (shadow)
> *Mỗi thanh ghi đoạn thực ra có 2 phần: phần nhìn thấy (selector 16-bit) và một phần ẩn CPU tự nạp.*

- 6 thanh ghi đoạn: **CS** (code), **DS** (data), **SS** (stack), **ES/FS/GS** (data phụ).
- Khi nạp selector vào thanh ghi, CPU **đọc descriptor 1 lần** rồi cache base/limit/quyền vào **phần ẩn
  (hidden/descriptor cache)**. Truy cập sau đó dùng cache, không tra GDT lại → nhanh. (Đổi GDT sau khi nạp
  mà không reload segment ⇒ vẫn dùng giá trị cũ.)
- **Ở long mode:** CS/DS/SS/ES base bị ép = 0, limit bỏ qua. **Nhưng FS/GS đặc biệt**: base của chúng
  **vẫn dùng được** và lấy từ MSR `IA32_FS_BASE` / `IA32_GS_BASE` (chứ không từ descriptor). OS dùng GS base
  để trỏ tới per-CPU/per-thread data (`swapgs` khi vào kernel). → liên quan [[Step 15 - Fast Syscalls]].

## 11. Nạp GDT trong thực tế — vì sao phải "nhảy xa"
> *Nạp `lgdt` xong, CS vẫn trỏ descriptor cũ. CS là thanh ghi đặc biệt: KHÔNG `mov` thẳng được. Phải dùng
> một cú "far jump"/"far return" để CPU nạp lại CS từ selector mới.*

Quy trình chuẩn (khớp cavOS `gdt_reload`, xem [[Step 06 - GDT & TSS]] §6.4):
```asm
lgdt [gdtr]            ; 1. nạp địa chỉ + limit bảng
                      ; 2. reload CS bằng far return:
push 0x28             ;    selector kernel code 64 (= offset 40)
lea  rax, [rip+1f]    ;    địa chỉ đích
push rax
lretq                 ;    "far return" → pop RIP + CS → CS = 0x28
1:                    ; 3. reload các segment data thường (mov được)
mov ax, 0x30          ;    kernel data 64
mov ds, ax            ;    ds/es/fs/gs/ss
```
- **CS chỉ đổi được qua control-flow xa**: `ljmp`/`lcall`/`lretq`/`iretq`. Mẹo `push selector; push rip;
  lretq` là cách phổ biến trong code 64-bit (không có `ljmp` tiện như 32-bit).
- DS/ES/SS/... thì `mov` trực tiếp được. Ở long mode chúng gần như vô nghĩa nhưng vẫn nên set cho sạch
  (tránh selector rác gây #GP khi `iretq`).

## 12. GDT 32-bit vs 64-bit — bảng đối chiếu nhanh
> *Tóm cái khác biệt cốt lõi để khỏi nhầm khi đọc code/spec.*

| Khía cạnh           | Protected mode (32-bit)              | Long mode (64-bit)                          |
| ------------------- | ------------------------------------ | ------------------------------------------- |
| base/limit code/data | **Có hiệu lực** (phân vùng thật)    | **Bỏ qua** (base=0, limit=∞ — phẳng)        |
| Cờ quyết định size  | D/B (16/32-bit)                      | **L=1** cho code 64-bit (D/B=0)             |
| GDTR base           | 32-bit                               | 64-bit                                      |
| TSS descriptor      | 8 byte                               | **16 byte** (cần base 64-bit)               |
| FS/GS base          | từ descriptor                        | từ **MSR** (`IA32_FS/GS_BASE`), descriptor bỏ |
| Còn dùng để làm gì  | phân vùng + bảo vệ + ring            | chủ yếu **khai báo ring (CPL) + code/data** |
| Vẫn bắt buộc?       | Có                                   | **Có** (CPU không chạy không có GDT)        |

## 🔗 Liên hệ trong vault
- Cách cavOS dựng & dùng GDT (code cụ thể): [[Step 06 - GDT & TSS]].
- Vì sao long mode làm segmentation teo lại: [[Long Mode]].
- Cơ chế thay thế segmentation để phân vùng/bảo vệ bộ nhớ: [[Paging]].
- IDT tham chiếu selector GDT + dùng IST/RSP0: [[Step 08 - IDT & Interrupts]].
- syscall/sysret + GS base per-CPU: [[Step 15 - Fast Syscalls]].

## 📖 Ánh xạ spec
- [[Intel SDM]] Vol.3A:
  - §3 *Protected-Mode Memory Management* — segment descriptor, selector, GDTR/LDTR.
  - §5 *Protection* — CPL/DPL/RPL, privilege checks, conforming code.
  - §7 *Task Management* — TSS, TSS descriptor, IST (long mode).
  - §3.x / Vol.3A §5.x — *IA-32e mode* (flat segmentation, bit L, FS/GS base MSR).
- AMD64 APM Vol.2 §4 (Segmented Virtual Memory) — bản mô tả long mode rõ ràng, dễ đọc song song SDM.
