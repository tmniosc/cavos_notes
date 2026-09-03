---
tags: [moc, home]
---

# 🏠 cavOS Study Vault

Vault ghi chú khi đọc source **cavOS** và ánh xạ với spec nền tảng x86_64.
Mở folder này bằng **Obsidian** (Open folder as vault). Các note liên kết bằng `[[wikilink]]`.

> Repo code: `~/cavOS` · Build: `make disk && make qemu` (xem [[Build & Debug Cheatsheet]])
>
> 🧠 **Context + bộ nhớ + tiến độ ở [[CLAUDE]]** (tự nạp khi mở Claude tại thư mục này).

> [!warning] 2026-09-03 — mất toàn bộ source, đang dựng lại
> Máy cũ mất → mất cả `~/cavOS` lẫn `~/oskernel-lab`. **Vault này còn nguyên** (chỉ vault sống sót).
> cavOS clone lại được 100%; source lab đã **viết lại từ note** và cất tại `Labs/src/oskernel-lab/`,
> **chưa build/boot lại lần nào** → output cũ trong các note Lab vẫn là của máy cũ.
> Đọc [[Khôi phục source (2026-09-03)]] trước khi chạy bất kỳ lệnh nào.

## 🚀 Bắt đầu từ đây
- [[Boot Flow (_start)]] — bản đồ thứ tự khởi tạo, xương sống của cả hành trình
- [[Spec Library]] — danh mục tài liệu spec & nơi tải

## 📚 Các bước học (theo `_start()`)
> 🔬 **Lab** (`~/oskernel-lab`): tự viết kernel từ số 0, song song. Gộp theo **cụm "chạy thấy được"**,
> KHÔNG map 1:1 với step (nhiều step phụ thuộc nhau → lab riêng sẽ rỗng). Tiêu chí: *1 lab = 1 thứ
> quan sát được*. Cột **Lab** bên dưới = lab phụ trách step đó.

| #   | Step                                   | Lab                                    | Tiến độ |
| --- | -------------------------------------- | -------------------------------------- | ------- |
| 00  | [[Step 00 - Boot & Limine]]            | [[Lab 0x00 - Hello Serial\|0x00]]      | ✅       |
| 01  | [[Step 01 - Serial UART]]              | [[Lab 0x00 - Hello Serial\|0x00]]      | ✅       |
| 02  | [[Step 02 - Bootloader Parser]]        | [[Lab 0x01 - Bootloader Parser\|0x01]] | ✅       |
| 03  | [[Step 03 - Framebuffer & Console]]    | [[Lab 0x02 - Framebuffer\|0x02]]       | ✅       |
| 04  | [[Step 04 - Physical Memory Manager]]  | [[Lab 0x03 - PMM & VMM\|0x03]]         | ✅       |
| 05  | [[Step 05 - Virtual Memory & Paging]]  | [[Lab 0x03 - PMM & VMM\|0x03]]         | ✅       |
| 06  | [[Step 06 - GDT & TSS]]                | 0x04                                   | ✅       |
| 07  | [[Step 07 - ACPI]]                     | 0x04                                   | ⬜       |
| 08  | [[Step 08 - IDT & Interrupts]]         | 0x04                                   | ⬜       |
| 09  | [[Step 09 - APIC & Timer]]             | 0x04                                   | ⬜       |
| 10  | [[Step 10 - PS2 Keyboard & Mouse]]     | 0x05                                   | ⬜       |
| 11  | [[Step 11 - Multitasking & Scheduler]] | 0x06                                   | ⬜       |
| 12  | [[Step 12 - Networking]]               | 0x07+                                  | ⬜       |
| 13  | [[Step 13 - PCI & NIC]]                | 0x07+                                  | ⬜       |
| 14  | [[Step 14 - AHCI & Filesystems]]       | 0x07+                                  | ⬜       |
| 15  | [[Step 15 - Fast Syscalls]]            | 0x07+                                  | ⬜       |
| 16  | [[Step 16 - SSE & FPU]]                | 0x07+                                  | ⬜       |
| 17  | [[Step 17 - Userspace & ELF Loader]]   | 0x07+                                  | ⬜       |

## 🔬 Thực hành (`~/oskernel-lab`)
- [[Lab 0x00 - Hello Serial]] ✅ — boot → serial (Step 00+01)
- [[Lab 0x01 - Bootloader Parser]] ✅ — in HHDM/memmap (Step 02)
- [[Lab 0x02 - Framebuffer]] ✅ — vẽ pixel/chữ, xác nhận HHDM (Step 03)
- [[Lab 0x03 - PMM & VMM]] ✅ — bitmap cấp frame + tự dựng page table, mov cr3 (Step 04+05)

## 🧩 Khái niệm cốt lõi
- [[Paging]] 📌 — 5 ý nền tảng (nạp trước Step 03)
- [[Limine Protocol]]
- [[Request-Response Mechanism]]
- [[Higher-Half Kernel]]
- [[HHDM]]
- [[Long Mode]]
- [[x86 Segmentation]] — segment + 3 chế độ (real/protected/long), gỡ rối "nhiều thứ quá"
- [[GDT]] — segment descriptor table (chuyên sâu spec, bổ trợ Step 06)
- [[KASLR & PIE Kernel]]

## 📖 Spec notes
- [[Intel SDM]]
- [[UART 16550]]

## 🛠️ Meta
- [[Dựng môi trường chung]] — nền cho cả hai: WSL, vault ở đâu, quy tắc build trong `~`, gói dùng chung
- [[Dựng môi trường cavOS]] — nhánh cavOS: cross-compiler → `make disk` → `make qemu`
- [[Dựng môi trường oskernel-lab]] — nhánh lab: Limine v8.x → chép source từ vault → `make run`
- [[Build & Debug Cheatsheet]] — lệnh build/QEMU/GDB, cấu trúc `os.img`, lỗi thường gặp
- [[Khôi phục source (2026-09-03)]] — note **tạm**, xoá sau khi khôi phục xong
