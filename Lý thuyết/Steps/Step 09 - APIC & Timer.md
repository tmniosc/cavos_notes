---
tags: [step, apic, timer]
status: todo
---

# Step 09 — APIC & Timer

**Hàm:** `initiateAPIC()` · **Đầu vào:** MADT từ [[Step 07 - ACPI]]

## 🎯 Mục đích
**Có nguồn "nhịp tim" (timer interrupt) và bộ định tuyến ngắt hiện đại.** Local APIC timer phát ngắt định kỳ
→ làm **tick** để scheduler cướp quyền (preempt). IO APIC định tuyến IRQ thiết bị → vector IDT. Thay thế PIC
8259 cũ. Không có tick thì không có đa nhiệm theo thời gian ([[Step 11 - Multitasking & Scheduler]]).

## Mục tiêu học
- Local APIC (MMIO base), spurious vector, EOI.
- IO APIC: định tuyến IRQ → vector.
- Local APIC timer (one-shot/periodic) làm tick scheduler.
- Tắt PIC 8259 cũ (mask).

## Câu hỏi mở
- [ ] Calibrate APIC timer bằng gì (PIT/HPET/TSC)?
- [ ] x2APIC hay xAPIC (MMIO)?

## Ánh xạ spec
- [[Intel SDM]] Vol.3 — APIC. ACPI MADT.
