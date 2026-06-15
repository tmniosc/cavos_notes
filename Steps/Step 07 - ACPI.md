---
tags: [step, acpi, firmware]
status: todo
---

# Step 07 — ACPI

**Hàm:** `initiateACPI()` · **Đầu vào:** `rsdp` từ [[Step 02 - Bootloader Parser]]

## 🎯 Mục đích
**Đọc các bảng mô tả phần cứng do firmware cung cấp** (qua RSDP từ [[Step 02 - Bootloader Parser]]). ACPI là
"danh bạ" của bo mạch: có bao nhiêu CPU, APIC ở đâu, HPET/PCIe ở đâu, cách tắt/sleep máy. Cụ thể cavOS cần
**MADT** để biết Local/IO APIC → tiền đề bắt buộc cho [[Step 09 - APIC & Timer]].

## Mục tiêu học
- Từ RSDP → RSDT/XSDT → tìm các bảng (MADT/APIC, FADT, HPET…).
- Parse **MADT** lấy local APIC / IO APIC (chuẩn bị [[Step 09 - APIC & Timer]]).
- Checksum bảng ACPI.

## Câu hỏi mở
- [ ] RSDT (32-bit ptr) vs XSDT (64-bit ptr) — dùng cái nào?
- [ ] MADT entry types nào cần?

## Ánh xạ spec
- ACPI Specification (RSDP, XSDT, MADT).
