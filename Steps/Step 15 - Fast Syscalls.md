---
tags: [step, syscall, cpu]
status: todo
---

# Step 15 — Fast Syscalls (syscall/sysret)

**Hàm:** `initiateSyscalls()` · **Phụ thuộc:** GDT từ [[Step 06 - GDT & TSS]]

## 🎯 Mục đích
**Cho userspace (ring 3) gọi dịch vụ kernel (ring 0) nhanh.** Cài MSR cho lệnh `syscall`/`sysret` (nhanh hơn
ngắt). Khi user gọi syscall: chuyển sang stack kernel (`RSP0` từ TSS [[Step 06 - GDT & TSS]]), chạy handler,
trả về. Là **cầu nối** giữa chương trình người dùng và kernel — bắt buộc cho [[Step 17 - Userspace & ELF Loader]].

## Mục tiêu học
- MSR: `IA32_EFER.SCE`, `STAR`, `LSTAR`, `FMASK`.
- Entry `syscall` → lưu user context, đổi sang kernel stack (TSS RSP0).
- Quy ước ABI: số syscall ở RAX, tham số RDI/RSI/RDX/R10/R8/R9.

## Câu hỏi mở
- [ ] swapgs để lấy kernel GS base?
- [ ] Bảng dispatch syscall đặt ở đâu?

## Ánh xạ spec
- [[Intel SDM]] Vol.3 — SYSCALL/SYSRET, MSR STAR/LSTAR/FMASK.
