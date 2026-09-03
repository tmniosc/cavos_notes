# CLAUDE.md — cavOS Study Vault (context + bộ nhớ)

> Vault ghi chú học **cavOS** (hobby OS x86_64). File này tự nạp khi mở Claude tại thư mục này —
> đọc đầu mỗi phiên để khôi phục context. Điều hướng vault: xem [[Home]] và [[Boot Flow (_start)]].

## Ngôn ngữ
**Trả lời bằng tiếng Việt.**

## Mục tiêu
Đọc source cavOS từ đầu, **ánh xạ code ↔ spec nền tảng x86_64** (Intel SDM, ACPI, PCI, AHCI, Limine,
datasheet thiết bị), đi tuần tự theo thứ tự khởi tạo trong `_start()` (xem [[Boot Flow (_start)]]).

## Vị trí (RẤT QUAN TRỌNG) — cập nhật 2026-09-03, MÁY MỚI
> Toàn bộ source đã mất cùng máy cũ; đang dựng lại. Đọc [[Khôi phục source (2026-09-03)]] trước khi
> tin bất kỳ đường dẫn cũ nào trong vault.

| Thứ                                                     | Đường dẫn                                              |
| ------------------------------------------------------- | ------------------------------------------------------ |
| **Vault ghi chú** (file này) — Windows ⭐ vault Claude CLI mở | `D:\tmniosc\cavos_notes` ← bản chính chủ duy nhất, là CWD khi chạy `claude` |
| Vault trên GitHub                                       | `https://github.com/tmniosc/cavos_notes` (remote `origin`) |
| **Bản lưu source lab** (chỉ để backup/push, KHÔNG build ở đây) | `D:\tmniosc\cavos_notes\Labs\src\oskernel-lab`   |
| **Source code cavOS** — trong **WSL** (KHÔNG di chuyển) | `~/cavOS` — clone lại từ `github.com/malwarepad/cavOS` |
| **Source lab** — trong **WSL**                          | `~/oskernel-lab` — chép từ `Labs/src/oskernel-lab`     |
| Cross toolchain                                         | `~/opt/cross/bin/x86_64-cavos-gcc` (dựng bằng `make tools`) |
| Limine binary                                           | `~/opt/limine` — branch **`v8.x-binary`** (v9+ đổi tên `kernel_path`, lệch note) |

WSL2 **Ubuntu-26.04**, user `tmniosc` → `/home/tmniosc` (cài lại 2026-09-03). Claude chạy phía Windows →
truy cập WSL qua `wsl.exe` hoặc UNC `\\wsl.localhost\Ubuntu-26.04\home\tmniosc\...`.
`sudo` **hỏi mật khẩu** → lệnh `sudo` phải do user tự chạy.

> Dựng môi trường từ số 0: [[Dựng môi trường chung]] (nền) → rồi rẽ [[Dựng môi trường cavOS]]
> hoặc [[Dựng môi trường oskernel-lab]]. [[Khôi phục source (2026-09-03)]] là note **tạm** — xoá khi xong.

## Cấu trúc thư mục vault (chia 2026-05-31)
```
cavos_notes/
├─ CLAUDE.md · Home.md · Boot Flow (_start).md   ← ở gốc
├─ Steps/        Step 00 → 17
├─ Labs/         Lab 0x00 → 0x03 · outputs/ (output thật) · src/ (SOURCE LAB — bản lưu)
├─ Khái niệm/    Paging, Limine Protocol, HHDM, Higher-Half, Long Mode, KASLR, Request-Response
├─ Spec/         Intel SDM, UART 16550, Spec Library
└─ Meta/         Build & Debug Cheatsheet · Dựng môi trường (chung/cavOS/oskernel-lab) · Khôi phục source
```
> `Labs/src/oskernel-lab/` = **bản lưu source lab** (thêm 2026-09-03 sau khi mất source). Chỉ để backup +
> push GitHub, **KHÔNG build ở đó**. `.gitattributes` ép `Labs/src/**` luôn **LF** (CRLF → lỗi `env: bash\r`).
> Wikilink `[[...]]` phân giải theo **tên file** → chia thư mục KHÔNG làm hỏng link.

## Quy ước làm việc
- **Đọc code** trong WSL (`~/cavOS`, hoặc UNC từ Windows). **Ghi note** vào vault này trên D: (đường dẫn Windows).
- Mỗi step: đọc code thật → giảng + ánh xạ spec → cập nhật note step tương ứng (`status: done`),
  cập nhật checklist trong [[Home]] và mục "Tiến độ" bên dưới.
- Dùng wikilink `[[...]]` và tags như các note hiện có.
- **KHÔNG dùng emoji/icon trong note** (chốt 2026-09-03 — user không thích). Note mới viết thuần chữ;
  heading là `## Mục đích`, `## Nói thật đơn giản`, không gắn icon. Callout thì dùng `> [!warning]` /
  `> [!note]` trần, không thêm hình. Note cũ còn icon thì dọn dần khi nào sửa tới, đừng thêm icon mới.
  Mũi tên `→`, ký tự vẽ bảng `├─►`, dấu tick `[x]` **không phải icon** — vẫn dùng bình thường.
- **Mỗi note step có 2 mục cố định gần đầu:** `## Mục đích` (step này để làm gì, link chéo step phụ thuộc)
  và `### Nói thật đơn giản` — **tóm tắt TOÀN BÀI** bằng ngôn ngữ đời thường (ẩn dụ, không thuật ngữ):
  đi qua hết các ý chính của step, chỗ nào quá kỹ thuật (bit cờ, macro, asm) thì lướt/bỏ. Người đọc chỉ cần
  đọc mục này là hiểu step làm gì & vì sao.
- **MỌI mục con (`##`) đều mở đầu bằng 1 câu *in nghiêng* giải thích đời thường** ("> *…*" hoặc `_…_`)
  trước khi vào code/chi tiết. User là người mới → mỗi mục phải đọc-là-hiểu, không chỉ riêng mục Nói thật đơn giản.
  Code/bit/asm vẫn giữ, nhưng luôn có câu dẫn dễ hiểu ở trên.
  Phần kỹ thuật chi tiết để bên dưới. Giữ đúng 2 mục này cho mọi step khi điền.
- **Lab gộp theo cụm "chạy thấy được"**, KHÔNG map 1:1 với step (xem bảng trong [[Home]]). Lab phải
  **build + boot QEMU thật** rồi mới chép output vào note — KHÔNG bịa output/bài học (đã từng sai, xem
  [[Lab 0x03 - PMM & VMM]] "Ghi chú trung thực").
- **Lab TÁCH MODULE từ Lab 0x02 trở đi**: `io.h`/`serial.{h,c}`/`boot.{h,c}`/`pmm.{h,c}`/`paging.{h,c}`...
  + `kernel.c` (chỉ orchestrate). `GNUmakefile` khai `SRCS := kernel.c <module>.c ...` (common.mk tự build).
  **String output trong code để TIẾNG ANH**; comment giải thích tiếng Việt OK. Bám sát cách cavOS thật làm,
  KHÔNG tự chế khác (vd PMM tính `mmTotal` = cộng dồn length ≠ RESERVED, y `bootloader.c`).
- **BACKUP SOURCE LAB (mới 2026-09-03, sau khi mất sạch source):** mỗi khi sửa code trong `~/oskernel-lab`,
  chép ngược về `Labs/src/oskernel-lab/` rồi commit + push. Vault là thứ DUY NHẤT sống sót lần mất máy vừa
  rồi → cái gì không nằm trong vault + GitHub thì coi như sẽ mất. Xem [[Khôi phục source (2026-09-03)]].

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

> Note nền tảng: [[Paging]] — 5 ý cốt lõi để đọc link.ld/memmap/HHDM (nạp trước Step 03).
> Thực hành song song: [[Lab 0x00 - Hello Serial]] (gộp Step 00+01) tại `~/oskernel-lab`.

## Trạng thái môi trường (2026-09-03) — ĐỌC TRƯỚC KHI CHẠY LỆNH
Kiến thức trong vault còn nguyên (Step 00 đến 06 vẫn xong), nhưng **môi trường thì trắng**:

- [x] Source lab viết lại từ note → `Labs/src/oskernel-lab/` (39 file)
- [x] WSL `Ubuntu-26.04` / `tmniosc`; apt xong; Limine 8.7.0 ở `~/opt/limine`; cavOS ở `~/cavOS`
- [x] **Build + boot thật cả 4 lab** (gcc hệ thống) — output thật đã chép vào note
- [ ] `make tools` (đang chạy) → `sudo -v && make disk` → `make qemu`
- [ ] Push `Labs/src/` lên GitHub

Chi tiết đầy đủ: [[Khôi phục source (2026-09-03)]].

## Quyết định đã chốt
- Dùng **vault Obsidian** (nhiều note liên kết) cho nội dung học; **memory + context gộp vào CLAUDE.md này** (1 file).
- Vault để trên **D: (Windows)**; code để trong **WSL**.
- **Source lab backup trong vault** (`Labs/src/`) và push GitHub — chốt sau vụ mất source 2026-09-03.
- Lab dùng **Limine v8.x-binary** + `limine.h` của cavOS (base revision 2), KHÔNG lên v9+.
