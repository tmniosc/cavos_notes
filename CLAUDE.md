# CLAUDE.md — cavOS Study Vault (context + bộ nhớ)

> Vault ghi chú học **cavOS** (hobby OS x86_64), viết bằng **HTML theme dark noir**. File này tự nạp khi mở
> Claude tại thư mục này — đọc đầu mỗi phiên để khôi phục context.
> Điều hướng: `docs/index.html` → `docs/ly_thuyet/ly_thuyet.html` · `docs/thuc_hanh/thuc_hanh.html` ·
> `docs/cong_cu_va_moi_truong/cong_cu_va_moi_truong.html`.

## Ngôn ngữ
**Trả lời bằng tiếng Việt.**

## Mục tiêu
Đọc source cavOS từ đầu, **ánh xạ code ↔ spec nền tảng x86_64** (Intel SDM, ACPI, PCI, AHCI, Limine,
datasheet thiết bị), đi tuần tự theo thứ tự khởi tạo trong `_start()` (xem `docs/ly_thuyet/bai/bai_00_bat_dau.html`).

## Vị trí (RẤT QUAN TRỌNG)
| Thứ | Đường dẫn |
| --- | --- |
| **Vault ghi chú** (file này) — Windows | `<vault>` — CWD khi chạy `claude`, khác nhau theo máy (bảng dưới) |
| Vault trên GitHub | `github.com/tmniosc/cavos_notes` (remote `origin`) |
| **Bản lưu source lab** (backup/push, KHÔNG build ở đây) | `<vault>\lab\src\oskernel-lab` (log chạy thật: `lab\outputs\`) |
| **Source cavOS** — trong WSL | `~/cavOS` — clone từ `github.com/malwarepad/cavOS` |
| **Source lab** — trong WSL | `~/oskernel-lab` — chép từ `lab/src/oskernel-lab` |
| Cross toolchain | `~/opt/cross/bin/x86_64-cavos-gcc` (GCC 11.4.0, dựng bằng `make tools`) |
| Limine binary | `~/opt/limine` — branch **`v8.x-binary`** (v9+ đổi tên `kernel_path`, lệch note) |
| Limine cavOS thật sự boot | `~/cavOS/src/bootloader/limine/` — `assert.sh` ghim 10.1.1 nhưng clone đầu nhánh `v10.x-binary`, binary là **10.8.5** (trang khái niệm trích source 10.8.5; `~/opt/limine` 8.7.0 chỉ cho oskernel-lab) |

WSL2 **Ubuntu-26.04** trên cả 2 máy. Claude chạy phía Windows → vào WSL qua `wsl.exe`
hoặc UNC `\\wsl.localhost\Ubuntu-26.04\home\<user>\...`. Nhận máy theo CWD lúc chạy `claude`:

| Máy | Vault (`<vault>`) | OS | User WSL | KVM | Chạy QEMU cavOS |
| --- | --- | --- | --- | --- | --- |
| Máy 1 | `D:\tmniosc\cavos_notes` | Windows 10 | `tmniosc` | không | `~/cavos-qemu.sh` |
| Máy 2 | `E:\tmniosc\#lab\computer_system\cavos_notes` | Windows 11 | `thinker` | **có** `/dev/kvm` | `make qemu` (sau khi `usermod -aG kvm`) |

> Máy 2 dựng ngày 2026-09-30: cavOS clone `2ba0edb`, Limine v8.7.0. `#` trong đường dẫn vault phải
> quote khi dùng trong shell (`"/mnt/e/tmniosc/#lab/..."`). Git trên máy 2 báo "dubious ownership" với vault
> → dùng `git -c safe.directory=E:/tmniosc/#lab/computer_system/cavos_notes ...`.

## Quy tắc chạy lệnh
- **Build trong `~` (ext4), TUYỆT ĐỐI không trên `/mnt/*` (`c` `d` `e`)** — `/mnt` là 9p nên `tar` rớt file,
  và ổ Windows biến `.sh` thành CRLF (`env: bash\r`). Chép `/mnt` → `~` là 1 chiều, chỉ file text.
- **`sudo` hỏi mật khẩu, cache gắn theo tty** → mọi lệnh `sudo` để user tự chạy; `sudo -v` gõ ở terminal
  khác không dùng chung được.
- `make tools` chạy **1 lần**. Vòng lặp thường ngày: `make disk && make qemu`.
- **`make tools` không chạy lại được**: `get_tools.sh` giải nén đè lên thư mục đã patch rồi patch lại →
  hỏi "already exists! Assume -R?". Muốn chạy lại thì xoá `tools/toolchain/temporarydir/{binutils-2.38,gcc-11.4.0}`
  trước (giữ tarball), đừng trả lời `y` cho câu hỏi của `patch`.
- **Máy 1 không có KVM** (Windows 10 → WSL2 không có nested virt): `make qemu` chết với `ENODEV`,
  dùng `~/cavos-qemu.sh` thay thế. Máy 2 có KVM, script đó vẫn tạo sẵn để dự phòng.
- Bảng lỗi đầy đủ + cách dựng từ máy trắng: `docs/cong_cu_va_moi_truong/dung_moi_truong_chung.html` →
  `dung_moi_truong_cavos.html` / `dung_moi_truong_oskernel_lab.html`. Lệnh hằng ngày: `cam_nang_build_debug.html`.
  Bản lưu `cavos-qemu.sh` nằm ở `scripts/`.

## Quy ước vault (HTML)
- **Bố cục gốc vault** — 4 thứ, không trộn:
  ```
  CLAUDE.md   context + bộ nhớ (Markdown)
  docs/       site HTML: index.html, assets/, ly_thuyet/, thuc_hanh/, cong_cu_va_moi_truong/
  lab/        src/oskernel-lab/ (source lab) + outputs/ (log chạy thật)
  scripts/    script tiện ích (cavos-qemu.sh)
  ```
- **Mọi note là 1 trang HTML theo skill `dark-noir-docs`** (`.claude/skills/dark-noir-docs/`): noir đen/trắng/xám,
  không bo góc, sidebar + mục lục tự sinh. Theme dùng chung ở `docs/assets/noir.css` + `noir.js`; sơ đồ site
  ở `docs/assets/site.js`. Mở đúp `docs/index.html` là đọc được, không cần server. Không còn Markdown/Obsidian
  (trừ file này và `lab/src/oskernel-lab/README.md` của source lab).
- **Thêm note mới**: chép khung từ một trang cùng khu (hoặc `assets/page.html` trong skill), đặt đúng
  `data-root` / `data-page`, rồi **thêm vào `docs/assets/site.js`** — thiếu là sidebar và Trước/Sau không thấy trang.
  Link giữa các trang là **đường dẫn tương đối** → đổi tên/dời file phải sửa mọi link trỏ tới nó (grep tên file).
- **Sửa theme**: sửa trong skill (`claude_setup/skills/dark-noir-docs/assets/`), chép sang `docs/assets/` của vault
  và `.claude/skills/dark-noir-docs/`.
- 3 khu: **Lý thuyết** (đọc hiểu) · **Thực hành** (tự viết chạy được) · **Công cụ và môi trường**.
  Mỗi khu có 1 trang chỉ mục cùng tên nằm trong chính khu đó (`docs/ly_thuyet/ly_thuyet.html`).
- **Tên thư mục + tên file HTML: không dấu, chữ thường, `_` giữa các từ** (`ly_thuyet/khai_niem/paging.html`,
  `bai/bai_06_virtual_memory_paging.html`). Tiêu đề hiển thị (h1, sidebar `t` trong site.js) vẫn tiếng Việt có dấu.
  Tiếng Việt cho nội dung; thuật ngữ (Paging, HHDM, GDT, Long Mode…) giữ tiếng Anh.
- **KHÔNG dùng emoji/icon.** Mũi tên `→`, ký tự vẽ bảng `├─►`, tick `✓ ✗` không tính là icon.
- **Sơ đồ là SVG inline trong trang, không dùng ASCII art.** Vẽ theo style skill `dark-native-diagrams`
  (box vuông góc, mũi tên ngang/dọc, chữ sáng trên nền tối) nhưng **không lưu file `Diagram/` và không
  nhúng `![[...]]`** — dán thẳng `<svg>` vào `<figure class="diagram">`. Mỗi SVG: `id` gốc riêng
  (`svg-<tên>`), mọi id bên trong có tiền tố đó, CSS `<style>` scope dưới `#svg-<tên>`, chỉ dùng màu nền tối
  (xem mục figure trong SKILL.md của `dark-noir-docs`).
- **Chữ trong sơ đồ để TIẾNG ANH** (title, label, caption) dù note viết tiếng Việt.
- **Giữ nguyên dạng text**: cây thư mục (`├── └──`), code, bảng, và block output thật.
  Chỉ chuyển sơ đồ thật (flow, memory map, bit layout, layer stack, cây box). Còn ~39 khối `text` cũ, một
  phần là sơ đồ ASCII chưa vẽ lại — vẽ lại khi sửa tới trang đó.
- Sửa trang xong **mở trong browser xem bằng mắt** (chạy `py -3 -m http.server` trong vault rồi mở `/docs/`, preview
  `file://` của pane chỉ là snapshot không có CSS) và chạy bộ kiểm tra trong SKILL.md: không tràn ngang
  ở 375px, không anchor hỏng, không trùng id — lỗi hay gặp là nhãn mũi tên đè box và chữ tràn khung SVG.

## Quy ước viết note
- **Đọc code** trong WSL. **Ghi note** vào vault (ổ Windows).
- **Site là MỘT khoá học tuyến tính** (2026-10-03): Bài 0 → Bài 18 ở `docs/ly_thuyet/bai/bai_NN_<tên>.html`,
  đúng thứ tự `_start()`. Sidebar (site.js) và nút Trước/Sau đi theo thứ tự đó. Khái niệm (`khai_niem/`) là phần
  **tra cứu**, không nằm trên đường chính; Tài liệu gốc để biết spec nào nói phần nào.
- **Mỗi bài theo đúng khuôn 7 mục, theo thứ tự:** `Mục đích` (`#muc-dich`) → `Nói thật đơn giản` (`.card-head`,
  toàn bài bằng lời đời thường, có ẩn dụ) → `Cần biết trước` (`#can-biet-truoc`: 2–4 khái niệm giải thích ngắn tại chỗ
  + link "đọc sâu" sang `khai_niem/`; khái niệm đã dạy ở bài trước thì chỉ ghi "đã gặp ở Bài K") → `Đọc code cavOS`
  (`#doc-code`: file/hàm theo thứ tự chạy, h3 cho từng phần) → `Tự làm` (`#tu-lam`: lab + lệnh + output thật; chưa có
  lab thì một việc tay đã chạy thử thật) → `Tự kiểm tra` (`#tu-kiem-tra`: 3–5 câu `<details class="quiz">`, style ở
  noir.css) → `Đọc thêm` (`#doc-them`: ánh xạ spec, câu hỏi mở đã trả lời, nhật ký lỗi, chi tiết sâu).
  Bài 0 là trang "bắt đầu ở đây" (khuôn bài, bản đồ khoá học, chuẩn bị máy), không theo khuôn 7 mục.
- Viết bài mới (Bài 8 trở đi): đọc code thật trong `~/cavOS` → viết theo khuôn → sửa chip/cột "Tình trạng" trong
  `docs/ly_thuyet/ly_thuyet.html` (dàn ý → đã viết), chuyển bài từ nhóm "Sắp tới" sang "Bài học" trong site.js,
  cập nhật bản đồ trong Bài 0 (ô xanh) và mục "Tiến độ học" bên dưới.
- **Mọi mục `h2` mở đầu bằng 1 câu đời thường** `<p class="lede"><em>…</em></p>` trước khi vào code/chi
  tiết — user là người mới, mỗi mục phải đọc-là-hiểu. Chi tiết kỹ thuật để bên dưới.
- Code viết dạng text đã escape trong `.code > .code-bar(c|bash|asm|ld|make|text) + pre > code`; `noir.js` tự tô màu.

## Quy ước lab
- **Gộp theo cụm "chạy thấy được"**, KHÔNG map 1:1 với bài (bảng trong `docs/thuc_hanh/thuc_hanh.html`).
- **Build + boot QEMU thật rồi mới chép output vào note** — KHÔNG bịa output/bài học
  (đã từng sai, xem `docs/thuc_hanh/lab_0x03_pmm_vmm.html` "Ghi chú trung thực"). Log đầy đủ để ở `lab/outputs/`.
- **Tách module ngay từ Lab 0x00**: `io.h`/`serial.{h,c}`/`boot.{h,c}`/`pmm.{h,c}`/`paging.{h,c}` +
  `kernel.c` chỉ orchestrate; `GNUmakefile` khai `SRCS := ...`. **String in ra để tiếng Anh**, comment
  tiếng Việt. Bám cách cavOS thật làm, KHÔNG tự chế khác (vd `mmTotal` = cộng dồn length ≠ RESERVED).
- **Sửa code xong chép ngược** về `lab/src/oskernel-lab/` rồi commit + push. Vault là thứ duy nhất
  lên GitHub → cái gì không nằm trong vault thì không có bản sao nào.

## Tiến độ học
Trang bài nằm ở `docs/ly_thuyet/bai/` (tên file dạng `bai_06_virtual_memory_paging.html`; Bài N = Step N-1 cũ).
- [x] Bài 1 - Boot & Limine — link.ld, _start, Limine protocol
- [x] Bài 2 - Serial UART — 16550, COM1
- [x] Bài 3 - Bootloader Parser — 6 request → struct `bootloader`; paging/HHDM/kernel_addr/memmap/SMP/RSDP
- [x] Bài 4 - Framebuffer & Console — Limine FB qua HHDM, BGRX 32bpp, PSF1 font, console con trỏ
- [x] Bài 5 - Physical Memory Manager — bitmap 1bit/frame 4KiB, tự host qua HHDM, first-fit + lastDeepFragmented
- [x] Bài 6 - Virtual Memory & Paging — TÁI DÙNG bảng Limine, ko tự mov cr3; VirtualMap lazy 4 tầng qua HHDM; invlpg; NX chưa dùng
- [x] Bài 7 - GDT & TSS — long mode bỏ base/limit, vẫn cần CPL+cờ L; lretq đổi CS; TSS chỉ giữ RSP0/IST
- [x] Bài 8 - ACPI — uACPI 3 bước, RSDP qua HHDM, MADT; lỗi: vòng MADT đọc lố 44 byte; lab 0x05
- [x] Bài 9 - IDT & Interrupts — idt.c set_idt_gate/set_idt, isr.asm stub + isr_common, handle_interrupt; remap+tắt PIC, spurious APIC; không IST; lab 0x04
- [x] Bài 10 - APIC & Timer — initiateAPIC/ioApicRedirect/initiateApicTimer, calibrate 10 tick PIT; lỗi: irqPerCoreAllocate gán thay so sánh, timerTicks không volatile; lab 0x05
- [x] Bài 11 - PS2 Keyboard & Mouse — initiateKb/kbIrq/handleKbEvent, kbEvdevGenerate, initiateMouse/mouseIrq, /dev/input + /dev/stdin; lỗi: Caps/Shift/mũi tên, init race 0x20 bị kbIrq ăn; lab 0x06

> Nạp trước Bài 4: `docs/ly_thuyet/khai_niem/paging.html` — 5 ý cốt lõi để đọc link.ld/memmap/HHDM.

## Quyết định đã chốt
- Nội dung học là **site HTML dark noir** (chuyển từ vault Obsidian ngày 2026-09-30 bằng
  `dark-noir-docs/scripts/obsidian_to_noir.py`; bản Markdown cũ còn trong lịch sử git).
  **Context + bộ nhớ gộp vào CLAUDE.md này** (1 file, vẫn là Markdown).
- Vault trên **ổ Windows**; code trong **WSL**.
- **Source lab backup trong vault** (`lab/src/`) và push GitHub; docs HTML (`docs/`) tách riêng khỏi code.
- Lab dùng **Limine v8.x-binary** + `limine.h` của cavOS (base revision 2), KHÔNG lên v9+.
