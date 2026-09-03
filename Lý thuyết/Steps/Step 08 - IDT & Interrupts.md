---
tags: [step, cpu, interrupt]
status: todo
---

# Step 08 — IDT & Interrupts

**Hàm:** `initiateIDT()` · **Spec:** [[Intel SDM]] Vol.3 (interrupt/exception)

## 🎯 Mục đích
**Cho CPU biết chạy hàm nào khi có ngắt/ngoại lệ.** IDT là bảng 256 vector → handler. Không có nó, mọi lỗi
CPU (#PF, #GP, chia 0...) hay ngắt thiết bị (bàn phím, timer) đều khiến máy treo/reset. Đây là cơ chế để
kernel **giành lại quyền điều khiển** khỏi code đang chạy — nền cho timer ([[Step 09 - APIC & Timer]]),
preempt scheduler ([[Step 11 - Multitasking & Scheduler]]), và xử lý page fault.

## Mục tiêu học
- Dựng IDT 256 entry (gate descriptor), `lidt`.
- ISR stub (đẩy errcode/vector, lưu register, gọi handler C).
- Xử lý exception CPU (#PF, #GP, #DF…) — đọc CR2 cho page fault.

## Câu hỏi mở
- [ ] Interrupt gate vs trap gate (IF bit)?
- [ ] Liên hệ IST trong [[Step 06 - GDT & TSS]].

## Ánh xạ spec
- [[Intel SDM]] Vol.3 — IDT, exception vectors (0–31).
