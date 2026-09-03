---
tags: [step, userspace, elf]
status: todo
---

# Step 17 — Userspace & ELF Loader

**Files:** `entry/*` (elf loader), `multitasking/*` · **Spec:** ELF64; x86-64 psABI; musl

## 🎯 Mục đích
**Chạy chương trình của người dùng ở ring 3** — đích đến của cả hành trình. Đọc file ELF từ đĩa
([[Step 14 - AHCI & Filesystems]]), map các segment vào address space user ([[Step 05 - Virtual Memory & Paging]]),
dựng stack + argv/auxv, rồi nhảy xuống ring 3. Gom mọi thứ trước đó: paging (không gian riêng), GDT user
segment ([[Step 06 - GDT & TSS]]), syscall ([[Step 15 - Fast Syscalls]]), scheduler ([[Step 11 - Multitasking & Scheduler]]).
Kết quả: `_start` kết thúc bằng `run("/bin/bash")` — một shell thật chạy trên cavOS.

- `_start` kết thúc bằng `while(1) run("/bin/bash", ...)`.
- ELF loader: đọc file ([[Step 14 - AHCI & Filesystems]]), map các **PT_LOAD** vào không gian ảo
  ([[Step 05 - Virtual Memory & Paging]]), dựng stack + **auxv**, nhảy vào **ring 3**.
- Dynamic linking: musl ld.so cho shared library.

## ✅ Câu hỏi
- auxv là gì, chứa entry nào (AT_PHDR, AT_ENTRY, AT_RANDOM...)?
- Chuyển vào ring 3 dùng `iret`/`sysret` thế nào? Stack ban đầu layout ra sao (argc/argv/envp/auxv)?

## 📝 Notes của tôi
>
