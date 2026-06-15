---
tags: [step, storage, fs]
status: todo
---

# Step 14 — AHCI & Filesystems

**Hàm:** `initiateAHCI()` · **Phụ thuộc:** PCI từ [[Step 13 - PCI & NIC]]

## 🎯 Mục đích
**Đọc/ghi đĩa thật (SATA qua AHCI) và hiểu dữ liệu trên đĩa thành file.** AHCI controller (tìm qua
[[Step 13 - PCI & NIC]]) cho phép DMA sector; filesystem biến sector thô thành cây thư mục/file. Nền để nạp
chương trình userspace từ đĩa ([[Step 17 - Userspace & ELF Loader]]).

## Mục tiêu học
- AHCI controller (ABAR), port, command list, FIS, PRDT.
- Đọc/ghi sector (SATA identify, DMA).
- Filesystem trên đĩa (FAT? ext2? — xác định khi đọc code).

## Câu hỏi mở
- [ ] Cơ chế command slot & completion?
- [ ] FS layout cavOS dùng?

## Ánh xạ spec
- AHCI 1.3.1 spec; SATA/ATA command set.
