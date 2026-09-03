---
tags: [moc, lythuyet]
---

# Lý thuyết

> _Nửa "đọc và hiểu" của vault: đi theo thứ tự khởi tạo của cavOS, mỗi bước ánh xạ code thật với spec
> nền tảng x86_64. Nửa còn lại là [[Thực hành]] — tự viết lại để kiểm chứng._

Về trang chủ: [[Home]]

## Bắt đầu từ đâu

1. [[Boot Flow (_start)]] — bản đồ thứ tự khởi tạo, xương sống của cả hành trình. Mọi Step đều treo vào đây.
2. [[Paging]] — 5 ý nền tảng, nên nạp trước Step 03 thì đọc `link.ld` / memmap / HHDM mới trôi.
3. [[Danh mục tài liệu]] — học phần nào thì mở tài liệu gốc nào, lấy ở đâu.

## Các bước theo `_start()`

> _Mỗi Step = một mảnh trong hàm khởi tạo của cavOS, đọc theo đúng thứ tự máy chạy._
> Cột **Lab** là bài thực hành phụ trách step đó, chi tiết ở [[Thực hành]].

| #   | Step                                   | Lab   | Tiến độ   |
| --- | -------------------------------------- | ----- | --------- |
| 00  | [[Step 00 - Boot & Limine]]            | 0x00  | xong      |
| 01  | [[Step 01 - Serial UART]]              | 0x00  | xong      |
| 02  | [[Step 02 - Bootloader Parser]]        | 0x01  | xong      |
| 03  | [[Step 03 - Framebuffer & Console]]    | 0x02  | xong      |
| 04  | [[Step 04 - Physical Memory Manager]]  | 0x03  | xong      |
| 05  | [[Step 05 - Virtual Memory & Paging]]  | 0x03  | xong      |
| 06  | [[Step 06 - GDT & TSS]]                | 0x04  | xong      |
| 07  | [[Step 07 - ACPI]]                     | 0x04  | tiếp theo |
| 08  | [[Step 08 - IDT & Interrupts]]         | 0x04  |           |
| 09  | [[Step 09 - APIC & Timer]]             | 0x04  |           |
| 10  | [[Step 10 - PS2 Keyboard & Mouse]]     | 0x05  |           |
| 11  | [[Step 11 - Multitasking & Scheduler]] | 0x06  |           |
| 12  | [[Step 12 - Networking]]               | 0x07+ |           |
| 13  | [[Step 13 - PCI & NIC]]                | 0x07+ |           |
| 14  | [[Step 14 - AHCI & Filesystems]]       | 0x07+ |           |
| 15  | [[Step 15 - Fast Syscalls]]            | 0x07+ |           |
| 16  | [[Step 16 - SSE & FPU]]                | 0x07+ |           |
| 17  | [[Step 17 - Userspace & ELF Loader]]   | 0x07+ |           |

## Khái niệm cốt lõi

> _Thứ dùng đi dùng lại ở nhiều Step, tách riêng để khỏi giải thích lại mỗi lần._

- [[Paging]] — nền tảng nhất, đọc trước
- [[Long Mode]] — chế độ 64-bit, nơi kernel thực sự chạy
- [[Higher-Half Kernel]] — vì sao kernel nằm ở nửa cao không gian ảo
- [[HHDM]] — cửa sổ nhìn toàn bộ RAM vật lý, và chuyện hai địa chỉ ảo trỏ một chỗ
- [[Limine Protocol]] · [[Request-Response Mechanism]] — cách kernel hỏi bootloader
- [[KASLR & PIE Kernel]] — vì sao địa chỉ vật lý đổi mỗi lần boot
- [[x86 Segmentation]] — segment + 3 chế độ (real/protected/long)
- [[GDT]] — segment descriptor table, bổ trợ Step 06

## Tài liệu gốc

> _x86 PC không có một cuốn TRM duy nhất như SoC ARM — nó là tổ hợp nhiều spec rời._

- [[Danh mục tài liệu]] — bảng tra tổng: cần gì thì mở cuốn nào
- [[Intel SDM]] — Intel 64 & IA-32 Software Developer's Manual
- [[UART 16550]] — datasheet con chip serial dùng ở Step 01
