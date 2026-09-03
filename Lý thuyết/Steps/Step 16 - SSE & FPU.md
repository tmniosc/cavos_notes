---
tags: [step, cpu, fpu]
status: todo
---

# Step 16 — SSE & FPU

**Hàm:** `initiateSSE()` · **Spec:** [[Intel SDM]] Vol.1/Vol.3

## 🎯 Mục đích
**Bật đơn vị tính dấu phẩy động / vector (SSE/FPU)** để code dùng float/SIMD chạy được mà không fault. Mặc
định long mode chưa bật; phải set CR0/CR4. Và phải lưu/khôi phục trạng thái FPU khi đổi task
([[Step 11 - Multitasking & Scheduler]]) — bắt buộc trước khi chạy userspace dùng float ([[Step 17 - Userspace & ELF Loader]]).

## Mục tiêu học
- Bật SSE: CR0 (clear EM, set MP), CR4 (OSFXSR, OSXMMEXCPT).
- (Tùy) AVX qua XCR0/XSAVE.
- Lưu/khôi phục trạng thái FPU/SSE khi context switch (`fxsave`/`fxrstor`).

## Câu hỏi mở
- [ ] Lazy FP save hay eager?
- [ ] XSAVE area kích thước (CPUID)?

## Ánh xạ spec
- [[Intel SDM]] Vol.1 — SSE/AVX; Vol.3 — control register, XSAVE.
