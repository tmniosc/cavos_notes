---
tags: [concept, memory]
---

# Higher-Half Kernel

> Kernel được nạp ở **nửa cao** của không gian địa chỉ ảo. cavOS: `0xffffffff80000000` (xem `link.ld`).

## Vì sao nửa cao?
- Không gian ảo x86_64 (4-level) là **48-bit có dấu**, sign-extend lên 64-bit → địa chỉ hợp lệ
  ("canonical") chỉ ở 2 đầu: nửa thấp `0x0..00–0x0000_7fff_ffff_ffff` và nửa cao
  `0xffff_8000_..–0xffff_ffff_..`. Khoảng giữa là "lỗ" không dùng được. → xem [[Long Mode]].
- Quy ước: **kernel ở nửa cao, userspace ở nửa thấp** → mỗi tiến trình dùng chung mapping kernel,
  tách bạch rõ ràng, dễ bảo vệ.

## Vì sao đúng `0xffffffff80000000` (= −2GiB)?
Để dùng được **`-mcmodel=kernel`** của GCC: mọi địa chỉ tuyệt đối là offset 32-bit có dấu so với
điểm này → lệnh sinh ra ngắn & nhanh hơn.

## Trong code (`link.ld`)
```ld
. = 0xffffffff80000000;
kernel_start = .;
.text : { *(.text .text.*) } :text
```
Liên quan: [[HHDM]] (cũng ở nửa cao, vùng khác), [[KASLR & PIE Kernel]], [[Step 00 - Boot & Limine]].

## 🔗 Spec
[[Intel SDM]] Vol 3A §4.5 (canonical address / sign-extension); Limine `PROTOCOL.md` (Kernel Address — bắt buộc nửa cao).
