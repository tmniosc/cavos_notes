---
tags: [spec, cpu]
---

# Intel SDM (Software Developer's Manual)

> "Programming Manual" của CPU x86_64. Tải bản **combined volumes** PDF từ intel.com.
> (AMD64 APM là bản tương đương của AMD.)

## Cấu trúc
| Vol | Nội dung | Dùng cho step |
|---|---|---|
| **Vol 1** | Basic architecture, thanh ghi, FPU/SSE/AVX | [[Step 16 - SSE & FPU]] |
| **Vol 2** | Instruction Set Reference (A-Z), gồm SYSCALL/SYSRET, IN/OUT | [[Step 15 - Fast Syscalls]] |
| **Vol 3** | System Programming Guide (kernel quan tâm nhất) | hầu hết các step |
| **Vol 4** | Model-Specific Registers (MSR) | [[Step 15 - Fast Syscalls]], [[Step 09 - APIC & Timer]] |

## Các mục Vol 3 hay dùng (đánh dấu sẵn)
- §3 Protected-Mode Memory Management (segment descriptor) → [[Step 06 - GDT & TSS]]
- §4 Paging (4-level, entry format, NX) → [[Step 05 - Virtual Memory & Paging]]
- §5 Protection (privilege rings, SYSCALL §5.8.8) → [[Step 15 - Fast Syscalls]]
- §6 Interrupt & Exception Handling (IDT, gate, IST) → [[Step 08 - IDT & Interrupts]]
- §8 Task Management (TSS, RSP0, IST stacks) → [[Step 06 - GDT & TSS]], [[Step 11 - Multitasking & Scheduler]]
- §9 Processor Management & Init (mode switch) → [[Long Mode]]
- §11 Advanced Programmable Interrupt Controller (APIC) → [[Step 09 - APIC & Timer]]
- §13 Managing State (FXSAVE/XSAVE) → [[Step 16 - SSE & FPU]]

> ⚠️ Số chương có thể lệch nhẹ giữa các phiên bản SDM — luôn kiểm mục lục bản bạn tải.
