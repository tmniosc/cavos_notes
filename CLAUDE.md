# CLAUDE.md — cavOS Study Vault (context + bộ nhớ)

> Vault ghi chú học **cavOS** (hobby OS x86_64). File này tự nạp khi mở Claude tại thư mục này —
> đọc đầu mỗi phiên để khôi phục context.
> Điều hướng: [[Home]] → [[Lý thuyết]] · [[Thực hành]] · [[Công cụ và môi trường]].

## Ngôn ngữ
**Trả lời bằng tiếng Việt.**

## Mục tiêu
Đọc source cavOS từ đầu, **ánh xạ code ↔ spec nền tảng x86_64** (Intel SDM, ACPI, PCI, AHCI, Limine,
datasheet thiết bị), đi tuần tự theo thứ tự khởi tạo trong `_start()` (xem [[Boot Flow (_start)]]).

## Vị trí (RẤT QUAN TRỌNG)
| Thứ | Đường dẫn |
| --- | --- |
| **Vault ghi chú** (file này) — Windows | `D:\tmniosc\cavos_notes` — CWD khi chạy `claude` |
| Vault trên GitHub | `github.com/tmniosc/cavos_notes` (remote `origin`) |
| **Bản lưu source lab** (backup/push, KHÔNG build ở đây) | `D:\tmniosc\cavos_notes\Thực hành\src\oskernel-lab` |
| **Source cavOS** — trong WSL | `~/cavOS` — clone từ `github.com/malwarepad/cavOS` |
| **Source lab** — trong WSL | `~/oskernel-lab` — chép từ `Thực hành/src/oskernel-lab` |
| Cross toolchain | `~/opt/cross/bin/x86_64-cavos-gcc` (GCC 11.4.0, dựng bằng `make tools`) |
| Limine binary | `~/opt/limine` — branch **`v8.x-binary`** (v9+ đổi tên `kernel_path`, lệch note) |

WSL2 **Ubuntu-26.04**, user `tmniosc` → `/home/tmniosc`. Claude chạy phía Windows → vào WSL qua `wsl.exe`
hoặc UNC `\\wsl.localhost\Ubuntu-26.04\home\tmniosc\...`.

## Quy tắc chạy lệnh
- **Build trong `~` (ext4), TUYỆT ĐỐI không trên `/mnt/c` `/mnt/d`** — `/mnt` là 9p nên `tar` rớt file,
  và ổ Windows biến `.sh` thành CRLF (`env: bash\r`). Chép `/mnt` → `~` là 1 chiều, chỉ file text.
- **`sudo` hỏi mật khẩu, cache gắn theo tty** → mọi lệnh `sudo` để user tự chạy; `sudo -v` gõ ở terminal
  khác không dùng chung được.
- `make tools` chạy **1 lần**. Vòng lặp thường ngày: `make disk && make qemu`.
- **Máy này không có KVM** (Windows 10 → WSL2 không có nested virt): `make qemu` chết với `ENODEV`,
  dùng `~/cavos-qemu.sh` thay thế.
- Bảng lỗi đầy đủ + cách dựng từ máy trắng: [[Dựng môi trường chung]] → [[Dựng môi trường cavOS]] /
  [[Dựng môi trường oskernel-lab]]. Lệnh hằng ngày: [[Cẩm nang build & debug]].

## Quy ước vault
- 3 khu: **Lý thuyết** (đọc hiểu) · **Thực hành** (tự viết chạy được) · **Công cụ và môi trường**.
  Mỗi khu có 1 trang chỉ mục cùng tên nằm trong chính khu đó.
- Tên thư mục + note mô tả để **tiếng Việt**; tên thuật ngữ (Paging, HHDM, GDT, Long Mode…) giữ tiếng Anh.
- Wikilink `[[...]]` phân giải theo **tên file** → dời thư mục KHÔNG làm hỏng link, nhưng **tên file phải
  duy nhất toàn vault**.
- **KHÔNG dùng emoji/icon trong note.** Heading trần (`## Mục đích`), callout `> [!warning]` không gắn hình.
  Note cũ còn icon thì dọn khi nào sửa tới. Mũi tên `→`, ký tự vẽ bảng `├─►`, tick `[x]` không tính là icon.
- **Sơ đồ dùng SVG, không dùng ASCII art.** Theo skill `dark-native-diagrams` (đã copy vào
  `.claude/skills/` của vault): nền tối / viền sáng / chữ sáng, `@media (prefers-color-scheme)` cho
  neutral, box vuông góc, mũi tên chỉ ngang hoặc dọc. Lưu `Diagram/<Tên note>/<tên>.svg`, nhúng
  `![[tên.svg]]` — **tên file phải duy nhất toàn vault**.
- **Chữ trong sơ đồ để TIẾNG ANH** (title, label, caption) dù note viết tiếng Việt.
- **Giữ nguyên dạng text**: cây thư mục (`├── └──`), code fence, bảng markdown, và block output thật.
  Chỉ chuyển sơ đồ thật (flow, memory map, bit layout, layer stack, cây box).
- Vẽ xong **mở SVG trong browser xem bằng mắt** trước khi báo xong — lỗi hay gặp là nhãn mũi tên đè lên
  box và chữ tràn ra ngoài khung.

## Quy ước viết note
- **Đọc code** trong WSL. **Ghi note** vào vault trên D:.
- Mỗi step: đọc code thật → giảng + ánh xạ spec → cập nhật note step (`status: done`), cập nhật bảng Step
  trong [[Lý thuyết]] và mục "Tiến độ học" bên dưới.
- **Mỗi note step có 2 mục cố định gần đầu:** `## Mục đích` (step này để làm gì, link chéo step phụ thuộc)
  và `## Nói thật đơn giản` — tóm tắt **toàn bài** bằng ngôn ngữ đời thường, có ẩn dụ, lướt chỗ quá kỹ
  thuật. Đọc riêng mục này là hiểu step làm gì và vì sao.
- **Mọi mục `##` mở đầu bằng 1 câu in nghiêng đời thường** (`> _…_`) trước khi vào code/chi tiết —
  user là người mới, mỗi mục phải đọc-là-hiểu. Chi tiết kỹ thuật để bên dưới.

## Quy ước lab
- **Gộp theo cụm "chạy thấy được"**, KHÔNG map 1:1 với step (bảng trong [[Thực hành]]).
- **Build + boot QEMU thật rồi mới chép output vào note** — KHÔNG bịa output/bài học
  (đã từng sai, xem [[Lab 0x03 - PMM & VMM]] "Ghi chú trung thực").
- **Tách module từ Lab 0x02 trở đi**: `io.h`/`serial.{h,c}`/`boot.{h,c}`/`pmm.{h,c}`/`paging.{h,c}` +
  `kernel.c` chỉ orchestrate; `GNUmakefile` khai `SRCS := ...`. **String in ra để tiếng Anh**, comment
  tiếng Việt. Bám cách cavOS thật làm, KHÔNG tự chế khác (vd `mmTotal` = cộng dồn length ≠ RESERVED).
- **Sửa code xong chép ngược** về `Thực hành/src/oskernel-lab/` rồi commit + push. Vault là thứ duy nhất
  lên GitHub → cái gì không nằm trong vault thì không có bản sao nào.

## Tiến độ học
- [x] [[Step 00 - Boot & Limine]] — link.ld, _start, Limine protocol
- [x] [[Step 01 - Serial UART]] — 16550, COM1
- [x] [[Step 02 - Bootloader Parser]] — 6 request → struct `bootloader`; paging/HHDM/kernel_addr/memmap/SMP/RSDP
- [x] [[Step 03 - Framebuffer & Console]] — Limine FB qua HHDM, BGRX 32bpp, PSF1 font, console con trỏ
- [x] [[Step 04 - Physical Memory Manager]] — bitmap 1bit/frame 4KiB, tự host qua HHDM, first-fit + lastDeepFragmented
- [x] [[Step 05 - Virtual Memory & Paging]] — TÁI DÙNG bảng Limine, ko tự mov cr3; VirtualMap lazy 4 tầng qua HHDM; invlpg; NX chưa dùng
- [x] [[Step 06 - GDT & TSS]] — long mode bỏ base/limit, vẫn cần CPL+cờ L; lretq đổi CS; TSS chỉ giữ RSP0/IST
- [ ] [[Step 07 - ACPI]] ← **tiếp theo** (boot log cavOS có sẵn phần uACPI 3.1.0 nạp 54 device để đối chiếu)
- [ ] Step 08 → 17: khung sẵn trong vault, điền dần.

> Nạp trước Step 03: [[Paging]] — 5 ý cốt lõi để đọc link.ld/memmap/HHDM.

## Quyết định đã chốt
- Nội dung học ở **vault Obsidian**; **context + bộ nhớ gộp vào CLAUDE.md này** (1 file).
- Vault trên **D: (Windows)**; code trong **WSL**.
- **Source lab backup trong vault** (`Thực hành/src/`) và push GitHub.
- Lab dùng **Limine v8.x-binary** + `limine.h` của cavOS (base revision 2), KHÔNG lên v9+.
