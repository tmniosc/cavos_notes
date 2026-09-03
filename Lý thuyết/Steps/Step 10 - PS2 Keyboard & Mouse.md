---
tags: [step, input]
status: todo
---

# Step 10 — PS/2 Keyboard & Mouse

**Hàm:** `initiatePS2()` · **Phụ thuộc:** IRQ từ [[Step 09 - APIC & Timer]], handler [[Step 08 - IDT & Interrupts]]

## 🎯 Mục đích
**Nhận input từ người dùng** (bàn phím, chuột) qua controller 8042 + ngắt. Là thiết bị input đầu tiên — biến
kernel từ "chỉ in ra" thành "tương tác được". Minh hoạ thực tế cách một driver dùng IRQ ([[Step 08 - IDT & Interrupts]],
[[Step 09 - APIC & Timer]]): thiết bị báo ngắt → handler đọc scancode → ánh xạ ký tự.

## Mục tiêu học
- Controller 8042 (port 0x60 data / 0x64 status+cmd).
- Keyboard: scancode set, IRQ1 → bản đồ ký tự.
- Mouse: bật aux device, gói 3/4 byte, IRQ12.

## Câu hỏi mở
- [ ] Scancode set 1 vs 2, translation?
- [ ] USB legacy emulation ảnh hưởng gì trên QEMU?

## Ánh xạ spec
- 8042 controller, PS/2 protocol (OSDev).
