---
tags: [step, pci, network]
status: todo
---

# Step 13 — PCI & NIC

**Hàm:** `initiatePCI()` · **Liên hệ:** [[Step 12 - Networking]], [[Step 14 - AHCI & Filesystems]]

## 🎯 Mục đích
**Tìm và nhận diện thiết bị cắm trên bus PCI** (NIC, AHCI controller...). Enumerate config space → đọc
vendor/device ID, class, BAR (vùng MMIO). Là "trình quản lý thiết bị" — bước bắt buộc trước khi điều khiển
bất kỳ phần cứng PCIe nào: NIC ([[Step 12 - Networking]]), đĩa ([[Step 14 - AHCI & Filesystems]]).

## Mục tiêu học
- Enumerate PCI (bus/device/function), config space (port 0xCF8/0xCFC hoặc ECAM).
- Đọc vendor/device ID, class code, BAR.
- Tìm NIC, map MMIO, đọc MAC.

## Câu hỏi mở
- [ ] Legacy config (CF8/CFC) vs MMIO ECAM (từ ACPI MCFG)?
- [ ] NIC cụ thể nào (e1000/rtl8139)?

## Ánh xạ spec
- PCI Local Bus Spec; datasheet NIC.
