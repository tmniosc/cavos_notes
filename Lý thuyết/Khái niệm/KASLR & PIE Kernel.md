---
tags: [concept, boot, security]
---

# KASLR & PIE Kernel

> Kernel cavOS là **ELF64 PIE** (Position-Independent Executable) → [[Limine Protocol|Limine]] có thể
> nạp nó ở địa chỉ vật lý/ảo **ngẫu nhiên** (KASLR) rồi tự sửa (relocate) các con trỏ.

## PIE là gì?
Mã không phụ thuộc địa chỉ nạp cố định. Các tham chiếu tuyệt đối được liệt kê trong section
`.dynamic` (relocation). Khi nạp ở base mới, loader cộng "slide" vào từng mục.

## Trong `link.ld`
```ld
PHDRS { ... dynamic PT_DYNAMIC FLAGS(...); }
.dynamic : { *(.dynamic) } :data :dynamic
```
`PT_DYNAMIC` cho Limine biết chỗ chứa relocation (kiểu phổ biến: `R_X86_64_RELATIVE`).

## KASLR = Kernel Address Space Layout Randomization
Nạp kernel ở base ngẫu nhiên mỗi lần boot → khó khai thác lỗ hổng. Vì base đổi, kernel cần biết
base thực qua request `kernel_address` của Limine:
```c
bootloader.kernelVirtBase = limineKrnres->virtual_base;
bootloader.kernelPhysBase = limineKrnres->physical_base;
```

Liên quan: [[Higher-Half Kernel]], [[Step 00 - Boot & Limine]], [[Step 02 - Bootloader Parser]].

## 🔗 Spec
ELF64 / System V ABI (program headers, `.dynamic`, relocation types); Limine `PROTOCOL.md` (Kernel Address).
