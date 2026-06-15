---
tags: [moc, spec]
---

# 📖 Spec Library

x86 PC **không có một TRM duy nhất** như SoC ARM — nó là tổ hợp nhiều spec.

| Vai trò | Tài liệu | Note / Nơi lấy |
|---|---|---|
| CPU "Programming Manual" | **Intel® 64 & IA-32 SDM** (Vol 1/2/3) | [[Intel SDM]] · intel.com |
| CPU (AMD) | **AMD64 APM** Vol 1-3 | amd.com developer |
| Firmware / nền tảng | **ACPI Specification** | uefi.org/specifications |
| | **UEFI Specification** | uefi.org/specifications |
| Bus | **PCI Local Bus 3.0** / **PCIe Base** | pcisig.com / OSDev wiki |
| Lưu trữ | **AHCI 1.3.1** + **SATA** | intel.com / sata-io.org |
| Boot protocol | **Limine `PROTOCOL.md`** | [[Limine Protocol]] · github limine |
| Nhị phân | **System V ABI** + **x86-64 psABI** + **ELF64** | gitlab.com/x86-psABIs |
| Datasheet thiết bị | UART **PC16550D** | [[UART 16550]] |
| | PS/2 **8042**, PIC **8259A**, PIT **8254** | OSDev wiki |
| | NIC **Intel E1000 (82540 SDM)**, **RTL8139/8169** | nhà sản xuất |
| Font | **PSF v1/v2** | kbd project |
| Filesystem | **FAT32 Spec**, **Ext2 docs** | hồ sơ public |

> 💡 **wiki.osdev.org** tóm tắt phần lớn spec trên kèm số hiệu thanh ghi/port. Dùng để định vị,
> rồi đọc spec gốc để chính xác.
