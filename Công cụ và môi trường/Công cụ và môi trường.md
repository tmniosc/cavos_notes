---
tags: [moc, meta]
---

# Công cụ và môi trường

> _Không phải kiến thức về hệ điều hành, mà là **thứ phải có thì mới học được**: máy dựng ra sao, build
> bằng lệnh gì, hỏng thì tra ở đâu._

Về trang chủ: [[Home]] · hai nửa nội dung: [[Lý thuyết]] · [[Thực hành]]

## Dựng máy từ con số 0

> _Làm theo thứ tự: nền chung trước, rồi mới rẽ nhánh. Hai nhánh độc lập, làm nhánh nào trước cũng được._

1. [[Dựng môi trường chung]] — WSL nằm đâu, vault nằm đâu, **quy tắc vàng build trong `~` chứ không `/mnt`**,
   gói `apt` cả hai nhánh đều cần.
2. [[Dựng môi trường cavOS]] — nhánh đọc code người ta: gói để đúc cross-compiler → `make tools` →
   `make disk` → `make qemu`.
3. [[Dựng môi trường oskernel-lab]] — nhánh code tự viết: `mtools` + Limine v8.x → chép source từ vault →
   `make run`. **Không cần** cross-compiler.

## Dùng hằng ngày

- [[Cẩm nang build & debug]] — lệnh build/QEMU/GDB, 4 file phân tích build (`.map/.dis/.sym/.elf.txt`),
  cấu trúc `os.img`, `compile_commands.json` cho clangd, và bảng lỗi thường gặp.
