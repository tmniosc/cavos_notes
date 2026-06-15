# CLAUDE.md — cavOS Study Vault (context + bộ nhớ)

> Vault ghi chú học **cavOS** (hobby OS x86_64). File này tự nạp khi mở Claude tại thư mục này —
> đọc đầu mỗi phiên để khôi phục context. Điều hướng vault: xem [[Home]] và [[Boot Flow (_start)]].

## Ngôn ngữ
**Trả lời bằng tiếng Việt.**

## Mục tiêu
Đọc source cavOS từ đầu, **ánh xạ code ↔ spec nền tảng x86_64** (Intel SDM, ACPI, PCI, AHCI, Limine,
datasheet thiết bị), đi tuần tự theo thứ tự khởi tạo trong `_start()` (xem [[Boot Flow (_start)]]).

## Vị trí (RẤT QUAN TRỌNG)
| Thứ                                                     | Đường dẫn                                              |
| ------------------------------------------------------- | ------------------------------------------------------ |
| **Vault ghi chú** (file này) — Windows ⭐ vault Claude CLI mở | `D:\1ABC\tmniosc\notes\cavos_notes` ← bản chính chủ duy nhất, là CWD khi chạy `claude` |
| **Source code cavOS** — trong **WSL** (KHÔNG di chuyển) | `~/cavOS` = `/home/tmnvi/cavOS`                        |
| Code xem từ Windows                                     | `\\wsl.localhost\Ubuntu-26.04\home\tmnvi\cavOS`        |
| Cross toolchain                                         | `~/opt/cross/bin/x86_64-cavos-gcc` (GCC 11.4.0)        |
| Repo gốc cũ trên ổ D (ĐỪNG build ở đây)                 | `D:\1ABC\@TMNIOSC\...\baremetal\cavOS`                 |

WSL2 **Ubuntu-26.04**, user `tmnvi`. Claude chạy phía Windows → truy cập WSL qua `wsl.exe` hoặc
UNC `\\wsl.localhost\Ubuntu-26.04\...`.

## Cấu trúc thư mục vault (chia 2026-05-31)
```
cavos_notes/
├─ CLAUDE.md · Home.md · Boot Flow (_start).md   ← ở gốc
├─ Steps/        Step 00 → 17
├─ Labs/         Lab 0x00, 0x01
├─ Khái niệm/    Paging, Limine Protocol, HHDM, Higher-Half, Long Mode, KASLR, Request-Response
├─ Spec/         Intel SDM, UART 16550, Spec Library
└─ Meta/         Build & Debug Cheatsheet
```
> Wikilink `[[...]]` phân giải theo **tên file** → chia thư mục KHÔNG làm hỏng link.

## Quy ước làm việc
- **Đọc code** trong WSL (`~/cavOS`, hoặc UNC từ Windows). **Ghi note** vào vault này trên D: (đường dẫn Windows).
- Mỗi step: đọc code thật → giảng + ánh xạ spec → cập nhật note step tương ứng (`status: done`),
  cập nhật checklist trong [[Home]] và mục "Tiến độ" bên dưới.
- Dùng wikilink `[[...]]` và tags như các note hiện có.
- **Mỗi note step có 2 mục cố định gần đầu:** `## 🎯 Mục đích` (step này để làm gì, link chéo step phụ thuộc)
  và `### 🧒 Nói thật đơn giản` — **tóm tắt TOÀN BÀI** bằng ngôn ngữ đời thường (ẩn dụ, không thuật ngữ):
  đi qua hết các ý chính của step, chỗ nào quá kỹ thuật (bit cờ, macro, asm) thì lướt/bỏ. Người đọc chỉ cần
  đọc mục này là hiểu step làm gì & vì sao.
- **MỌI mục con (`##`) đều mở đầu bằng 1 câu *in nghiêng* giải thích đời thường** ("> *…*" hoặc `_…_`)
  trước khi vào code/chi tiết. User là người mới → mỗi mục phải đọc-là-hiểu, không chỉ riêng mục 🧒.
  Code/bit/asm vẫn giữ, nhưng luôn có câu dẫn dễ hiểu ở trên.
  Phần kỹ thuật chi tiết để bên dưới. Giữ đúng 2 mục này cho mọi step khi điền.
- **Lab gộp theo cụm "chạy thấy được"**, KHÔNG map 1:1 với step (xem bảng trong [[Home]]). Lab phải
  **build + boot QEMU thật** rồi mới chép output vào note — KHÔNG bịa output/bài học (đã từng sai, xem
  [[Lab 0x03 - PMM & VMM]] "Ghi chú trung thực").
- **Lab TÁCH MODULE từ Lab 0x02 trở đi**: `io.h`/`serial.{h,c}`/`boot.{h,c}`/`pmm.{h,c}`/`paging.{h,c}`...
  + `kernel.c` (chỉ orchestrate). `GNUmakefile` khai `SRCS := kernel.c <module>.c ...` (common.mk tự build).
  **String output trong code để TIẾNG ANH**; comment giải thích tiếng Việt OK. Bám sát cách cavOS thật làm,
  KHÔNG tự chế khác (vd PMM tính `mmTotal` = cộng dồn length ≠ RESERVED, y `bootloader.c`).

## Bài học build (đừng lặp lại)
- **CHỈ build trong `~/cavOS` (ext4 native).** TUYỆT ĐỐI không build trên `/mnt/c` `/mnt/d`:
  - `/mnt` là 9p → `tar` rớt file (vd `opcode/i386.h`), build hỏng.
  - ổ Windows + `core.autocrlf=true` → script `.sh` thành CRLF → lỗi `env: bash\r`.
  - copy rootfs `target/` qua `/mnt` → hỏng (lỗi `Error relocating /bin/bash` readline).
- `make tools` chạy **1 lần** (lâu). Vòng lặp thường ngày: `make disk && make qemu`.
- `make disk` cần **sudo** ở bước `ports` (bootstrap Alpine) → chạy `sudo -v` trước, kẻo `sudo: timed out`.
- Chi tiết: [[Build & Debug Cheatsheet]].

## Tiến độ học
- [x] [[Step 00 - Boot & Limine]] — xong (link.ld, _start, Limine protocol)
- [x] [[Step 01 - Serial UART]] — xong (16550, COM1)
- [x] [[Step 02 - Bootloader Parser]] — xong (6 request → struct `bootloader`; paging/HHDM/kernel_addr/memmap/SMP/RSDP)
- [x] [[Step 03 - Framebuffer & Console]] — xong (Limine FB qua HHDM, BGRX 32bpp, PSF1 font, console con trỏ)
- [x] [[Step 04 - Physical Memory Manager]] — xong (bitmap 1bit/frame 4KiB, tự host qua HHDM, first-fit + lastDeepFragmented)
- [x] [[Step 05 - Virtual Memory & Paging]] — xong (TÁI DÙNG bảng Limine, ko tự mov cr3; VirtualMap lazy 4 tầng qua HHDM; invlpg; NX chưa dùng)
- [x] [[Step 06 - GDT & TSS]] — xong (long mode bỏ base/limit, vẫn cần CPL+cờ L; lretq đổi CS; TSS chỉ giữ RSP0/IST)
- [ ] [[Step 07 - ACPI]] ← **tiếp theo**
- [ ] Step 08 → 17: khung sẵn trong vault, điền dần.

> 📌 Note nền tảng: [[Paging]] — 5 ý cốt lõi để đọc link.ld/memmap/HHDM (nạp trước Step 03).
> 🔬 Thực hành song song: [[Lab 0x00 - Hello Serial]] (✅ gộp Step 00+01) tại `~/oskernel-lab`.

## Quyết định đã chốt
- Dùng **vault Obsidian** (nhiều note liên kết) cho nội dung học; **memory + context gộp vào CLAUDE.md này** (1 file).
- Vault để trên **D: (Windows)**; code để trong **WSL** (Ubuntu-26.04, giữ nguyên — đã build thành công).
