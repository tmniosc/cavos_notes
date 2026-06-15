---
tags: [step, scheduler, smp]
status: todo
---

# Step 11 — Multitasking & Scheduler

**Hàm:** `initiateTasking()` · **Đầu vào:** SMP từ [[Step 02 - Bootloader Parser]], tick từ [[Step 09 - APIC & Timer]]

## 🎯 Mục đích
**Chạy nhiều luồng/tiến trình "đồng thời" trên CPU.** Lưu/khôi phục context (register, stack, CR3), và dùng
timer tick ([[Step 09 - APIC & Timer]]) để **cướp quyền** (preempt) luân phiên các task. Đây là lúc `mov cr3`
thật sự được dùng để đổi address space ([[Step 05 - Virtual Memory & Paging]]) — mỗi task một không gian riêng.
Nền cho userspace ([[Step 17 - Userspace & ELF Loader]]).

## Mục tiêu học
- Task struct (context register, stack, address space).
- Context switch (lưu/khôi phục register, đổi CR3).
- Scheduler (round-robin?), preempt bằng timer interrupt.
- (Tùy) bring-up các AP core (SMP).

## Câu hỏi mở
- [ ] Lưu/khôi phục register ở đâu (trên stack hay TCB)?
- [ ] Đồng bộ giữa core (spinlock)?

## Ánh xạ spec
- [[Intel SDM]] Vol.3 — task, SMP/APIC INIT-SIPI.
