---
tags: [concept, cpu]
---

# Long Mode (64-bit)

> Chế độ hoạt động 64-bit của CPU x86_64. Khi [[Limine Protocol|Limine]] nhảy vào `_start`, CPU **đã ở
> long mode** và **paging đã bật** — kernel không phải tự chuyển chế độ.

## Các chế độ x86 (lịch sử)
> Gỡ rối đầy đủ "segment + 3 chế độ": [[x86 Segmentation]].
```
Real mode (16-bit) → Protected mode (32-bit) → Long mode (64-bit)
```
Bình thường bootloader phải đi qua từng nấc (bật A20, dựng GDT, bật PAE, set EFER.LME, bật paging,
set CR0.PG...). **Limine làm hết** → cavOS bỏ qua phần asm phức tạp này.

## Đặc điểm long mode (liên quan code cavOS)
- Địa chỉ ảo **canonical** 48-bit sign-extended → xem [[Higher-Half Kernel]].
- **Paging luôn bật**, dùng PAE 4-level (PML4) hoặc 5-level → xem [[Step 05 - Virtual Memory & Paging]].
- Segmentation gần như "phẳng": base/limit bị bỏ qua cho hầu hết segment → ảnh hưởng [[Step 06 - GDT & TSS]].
- Thanh ghi 64-bit (RAX...R15); calling convention theo x86-64 psABI.

## Bit/MSR liên quan (để tra khi cần)
- `CR0.PG` (paging), `CR0.PE` (protected), `CR4.PAE`, `EFER.LME`/`LMA` (long mode enable/active).

## 🔗 Spec
[[Intel SDM]] Vol 3A §9 (mode switching), §4 (paging), §3.x (segmentation in IA-32e).
