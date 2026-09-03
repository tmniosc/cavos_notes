---
tags: [meta, recovery, build, tạm]
status: tạm - sẽ xoá
---

# Khôi phục source (2026-09-03)

> [!warning] NOTE TẠM - SẼ XOÁ
> Note này chỉ sống trong lúc khôi phục. **Xoá khi mục "Việc còn lại" ở cuối tick hết**
> (đã build cavOS + boot 4 lab + chép output thật + push `Thực hành/src/` lên GitHub).
> Thứ cần giữ lại lâu dài đã nằm ở [[Dựng môi trường chung]] + [[Dựng môi trường cavOS]] +
> [[Dựng môi trường oskernel-lab]] (cách dựng) và [[CLAUDE]] (quy ước backup).
> Khi xoá nhớ gỡ luôn link trong [[Home]], [[CLAUDE]], [[Cẩm nang build & debug]] và 4 note Lab.

> _Toàn bộ source code (cavOS + lab) đã mất cùng cái máy cũ. Note này ghi lại: mất những gì,
> tìm ở đâu, khôi phục bằng cách nào, và chỗ nào **không** khôi phục nguyên bản được._

Liên quan: [[Dựng môi trường chung]] · [[Dựng môi trường cavOS]] · [[Dựng môi trường oskernel-lab]] ·
[[Cẩm nang build & debug]] · [[Home]] · [[CLAUDE]] ·
các note [[Lab 0x00 - Hello Serial]] → [[Lab 0x03 - PMM & VMM]].

## Nói thật đơn giản

> _Coi vault này là **quyển vở**, còn source code là **bài tập đã làm**. Máy cũ mất → mất hết bài tập,
> nhưng quyển vở vẫn còn nguyên vì nó nằm trên ổ D và đã đẩy lên GitHub._

Có 2 loại "bài tập" và số phận khác nhau:

- **cavOS** không phải bài tập của mình — đó là **sách giáo khoa của người khác**, mình chỉ đọc. Sách này
  bán ngoài hiệu (GitHub công khai), nên mất chỉ cần **mua lại quyển mới y hệt** → khôi phục 100%.
- **Lab** mới là bài tập mình tự làm → **không ai giữ hộ**. May là quyển vở ghi rất kỹ: làm gì, chia file
  ra sao, chạy ra kết quả gì. Nên phải **làm lại bài từ đầu, dựa vào vở**. Giống chép lại bài văn từ dàn ý
  chi tiết: nội dung đúng ý cũ, nhưng từng câu chữ chắc chắn không giống hệt bản gốc.

Rút ra: **vở còn thì làm lại được, nhưng làm lại tốn công**. Từ nay bài tập cũng phải cất lên GitHub như vở —
đó là lý do có thư mục `Thực hành/src/` trong vault.

## Đã tìm ở đâu (bằng chứng là thật sự không còn backup)

> _Trước khi chấp nhận "mất rồi, viết lại", phải lục hết mọi chỗ có thể còn bản sao. Đây là 4 chỗ đã lục._

| Chỗ tìm | Kết quả |
| --- | --- |
| Vault `D:\tmniosc\cavos_notes` | 47 file, **toàn `.md`** + 1 `.txt` output. Không có `.c/.h/Makefile` nào |
| GitHub `tmniosc` (API public repos) | 6 repo: `cavos_notes`, `bootsector`, `english_notes`, `hanyu_notes`, `mathscivinabook`, `xbee_ansic_library` → **không có** cavOS/oskernel-lab |
| Ổ D: | chỉ còn `D:\tmniosc\cavos_notes`. Đường dẫn cũ ghi trong [[CLAUDE]] là `D:\1ABC\...` — **đã biến mất** |
| WSL | máy mới lúc đó: `Ubuntu-24.04`, user `ioczst` — không có `~/cavOS`, `~/oskernel-lab`, `~/opt/cross`. (Sau đó gỡ để cài lại `Ubuntu-26.04`) |

→ Kết luận: **không tồn tại bản backup nào**. Hai phần phải khôi phục theo 2 cách khác hẳn nhau.

## Máy cũ vs máy mới (đường dẫn đã đổi hết)

> _Note cũ ghi đường dẫn theo máy cũ; sang máy này sai gần hết. Bảng này để khỏi lẫn khi đọc lại note cũ._

| Thứ | Máy cũ (ghi trong note cũ) | Máy hiện tại (2026-09-03) |
| --- | --- | --- |
| Vault | `D:\1ABC\tmniosc\notes\cavos_notes` | `D:\tmniosc\cavos_notes` |
| WSL distro | `Ubuntu-26.04` | `Ubuntu-26.04` (cài lại sạch 2026-09-03) |
| WSL user / home | `tmnvi` → `/home/tmnvi` | `tmniosc` → `/home/tmniosc` |
| Cross toolchain | `~/opt/cross/bin/x86_64-cavos-gcc` (GCC 11.4.0) | chưa có — dựng lại bằng `make tools` |
| Repo gốc cũ ổ D | `D:\1ABC\@TMNIOSC\...\baremetal\cavOS` | không còn |

> Mọi chỗ trong vault còn ghi `/home/tmnvi/...` (nhất là [[Cẩm nang build & debug]] mục cross tools)
> đều phải sửa lại theo user mới. `common.mk` bản dựng lại **không hard-code** nữa mà dùng `$(HOME)`.

## 1. cavOS — clone lại, nguyên bản 100%

> _cavOS là repo công khai của người khác, mình chỉ đọc chứ không sửa → clone về là y hệt bản cũ._

- Nguồn: `https://github.com/malwarepad/cavOS` (branch `master`).
- Cỡ: ~13 MB đủ lịch sử.
- Vị trí: `~/cavOS` **trong WSL ext4** — tuyệt đối không build trên `/mnt/c` `/mnt/d` (bài học 9p/CRLF
  trong [[Cẩm nang build & debug]]).

```bash
git clone https://github.com/malwarepad/cavOS.git ~/cavOS
```

> Vì mình chưa từng sửa code cavOS (chỉ đọc + ghi note), **không mất gì cả**. Tiến độ học nằm ở vault,
> không nằm trong repo.

## 2. Lab `~/oskernel-lab` — viết lại từ note, KHÔNG nguyên bản

> _Đây là code tự viết, không ai giữ hộ. Note tả rất kỹ kiến trúc và kết quả nhưng **không chứa full source**
> (chỉ vài snippet) → phải viết lại. Chạy đúng như cũ thì được, giống từng dòng thì không._

**Bản dựng lại đặt trong vault:** `Thực hành/src/oskernel-lab/` → commit chung repo `cavos_notes` để đẩy GitHub.
Đây chỉ là **bản lưu**; **build vẫn phải trong WSL** (`~/oskernel-lab`), không build trong `Thực hành/src`.

### Cây file dựng lại

```
Thực hành/src/oskernel-lab/
├─ common.mk           build chung: tự dò cross x86_64-cavos-gcc, không có thì lùi về gcc hệ thống
├─ linker.ld           bản gốc (mỗi project giữ 1 copy)
├─ limine.conf         timeout 0 · protocol limine · kernel_path boot():/boot/kernel.bin
├─ limine.h            lấy từ cavOS src/kernel/include/limine.h (đúng bản base revision 2)
├─ .clangd             gỡ flag GCC-only cho clangd
├─ scripts/mkimage.sh  os.img: MBR + FAT32@1MiB + limine bios-install (mtools, không cần sudo)
├─ 00-hello-serial/       kernel.c serial.{h,c} io.h linker.ld GNUmakefile
├─ 01-bootloader-parser/  + serial_puthex/putdec, 4 Limine request
├─ 02-framebuffer/        + fb.{h,c} (font 8x8 nhúng)
└─ 03-pmm-vmm/            + boot.{h,c} pmm.{h,c} paging.{h,c}
```

### Bám theo note ở những điểm nào

> _Không tự chế: mỗi lựa chọn dưới đây đều lấy từ 1 câu cụ thể trong note lab/cheatsheet._

| Điểm | Theo note nào |
| --- | --- |
| `ENTRY(kmain)` (không phải `_start`), `. = 0xffffffff80000000`, PHDRS **không ghi FLAGS**, `KEEP(*(.limine_requests))` | [[Lab 0x00 - Hello Serial]] "Các mảnh chính" |
| `LIMINE_BASE_REVISION(2)` — không dùng 3 | [[Lab 0x00 - Hello Serial]] + [[Limine Protocol]] §4 |
| `-Wl,-z,max-page-size=0x1000` để `ALIGN(MAXPAGESIZE)` ra biên `0x1000` | trích `kernel.map` trong [[Lab 0x00 - Hello Serial]] (rodata @ `…80001000`) |
| 4 artifact `.map/.dis/.sym/.elf.txt` sinh mỗi lần link | [[Cẩm nang build & debug]] bảng artifact |
| `put_pixel` nhảy hàng bằng **`pitch`**, format **BGRX**, font 8x8 nhúng thẳng | [[Lab 0x02 - Framebuffer]] "Khác với cavOS" |
| `mmTotal` = **cộng dồn length mọi vùng khác `RESERVED`** (không lấy max-end) | [[Lab 0x03 - PMM & VMM]] (bám `bootloader.c`) |
| Guard `if (first+i >= pmm_blocks) break;` trong `bm_mark` | [[Lab 0x03 - PMM & VMM]] |
| Trình tự PMM: `memset 0xff` → mở USABLE → đóng non-USABLE → tự mark bitmap | [[Lab 0x03 - PMM & VMM]] mục 1 |
| PML4 mới + copy 512 entry + `mov cr3`; `vmap` lazy 4 tầng + `invlpg`; `vresolve` | [[Lab 0x03 - PMM & VMM]] mục 2, 3 |
| Bảng `pmm_dump_map` 10 cột (region row + dòng con `>` + dòng gap) | `Thực hành/outputs/lab-0x03-run.txt` |
| String output **tiếng Anh**, comment tiếng Việt; `SRCS := ...` trong `GNUmakefile` | quy ước trong [[CLAUDE]] |
| Không để comment cùng dòng với `VAR := value` | [[Cẩm nang build & debug]] |

### Một chỗ CỐ Ý làm khác bản cũ (bug bitmap)

> _Bản cũ có lỗi nhỏ; chép lại y nguyên thì giữ luôn lỗi, nên đã vá. Hệ quả: vài con số trong note
> [[Lab 0x03 - PMM & VMM]] sẽ lệch khi chạy lại._

Bản cũ (đọc ngược từ output đã lưu): bitmap 8301 B đặt tại `0x60000` → trải **3 frame**
(`0x60000`, `0x61000`, `0x62000` — vì byte cuối ở `0x6206d`). Nhưng output cũ cho:

```
[pmm] alloc #1 PA  = 0x0000000000062000     <- alloc phát ra frame ĐANG chứa đuôi bitmap
[pmm] free frames  = 65196                  <- USABLE 65198 frame trừ đúng 2, đáng lẽ trừ 3
```

→ bản cũ chỉ đánh dấu **2 frame** (chia lấy phần nguyên), **để hở frame đuôi**. Lần đó không lộ ra vì
PML4 mới nằm ở `0x62000` chỉ dùng entry 192/256/511 (byte offset từ 1536 trở đi), không chạm 109 byte đầu
của frame — tức **may chứ không đúng**.

Bản dựng lại: `bm_mark(bitmap_pa, bytes, 1)` làm tròn **LÊN** → chiếm đủ 3 frame.

**Đã chạy thật 2026-09-03, xác nhận vá đúng:** lần boot này Limine xếp khác nên bitmap rơi vào `0x53000`
(`0x53000`–`0x5506d` = frame `0x53/0x54/0x55`), và `alloc #1` ra `0x56000` — **frame ngay sau** bitmap,
không giẫm lên nữa. Kiểm chéo: USABLE `76 + 65077 + 31 = 65184` frame, trừ 3 frame bitmap = `65181`,
đúng bằng `free frames` in ra. Output đầy đủ: `Thực hành/outputs/lab-0x03-run-2026-09-03.txt`.

> Con số dự đoán trước khi chạy (`alloc #1 = 0x63000`, `free = 65195`) **sai**, vì nó giả định memmap
> giống hệt lần cũ — thực tế Limine dời hết vùng mỗi lần boot. Ghi lại đây đúng tinh thần
> "Ghi chú trung thực": số phải lấy từ máy, không lấy từ suy đoán.

## Cần cài lại gì trên máy mới

> _Tóm tắt. Bản đầy đủ: [[Dựng môi trường chung]] → [[Dựng môi trường cavOS]] →
> [[Dựng môi trường oskernel-lab]]._

```bash
# 1. gói hệ thống (cần sudo — tự chạy, Claude không nhập mật khẩu hộ được)
sudo apt update && sudo apt install -y nasm dosfstools parted bison flex libgmp3-dev libmpc-dev \
  libmpfr-dev texinfo libisl-dev build-essential autoconf automake wget \
  qemu-system-x86 mtools xorriso curl bear clangd

# 2. Limine binary v8.x (khớp limine.conf kiểu kernel_path + base revision 2)
git clone -b v8.x-binary --depth 1 https://github.com/limine-bootloader/limine ~/opt/limine
make -C ~/opt/limine

# 3. cavOS
git clone https://github.com/malwarepad/cavOS.git ~/cavOS

# 4. lab: chép bản lưu từ vault sang WSL rồi build ở ext4 (KHÔNG build trong /mnt/d)
cp -r /mnt/d/tmniosc/cavos_notes/Thực hành/src/oskernel-lab ~/oskernel-lab
```

> **v8.x-binary chứ không phải v9+**: từ Limine 9, `kernel_path` đổi tên và
> `LIMINE_KERNEL_ADDRESS_REQUEST` bị thay bằng `EXECUTABLE_ADDRESS` → lệch hết với `limine.h` cavOS
> và với mọi note đã viết.

> Chép qua `/mnt/d` chỉ **1 chiều** (vault → WSL) và chỉ là file text nhỏ. Không bao giờ **build**
> hay giải nén rootfs trên `/mnt` — xem "Bài học build" trong [[CLAUDE]].

## Việc còn lại

- [x] Cài xong `Ubuntu-26.04`, user `tmniosc`
- [x] Clone cavOS → `~/cavOS`; chép source lab → `~/oskernel-lab`
- [x] Cài gói hệ thống + Limine `v8.x-binary` (Limine 8.7.0) → `~/opt/limine`
- [x] **Build + boot thật cả 4 lab**, chép output thật vào note (Lab 0x03 xác nhận đã vá bug bitmap)
- [x] Commit `Thực hành/src/` + note vào repo vault (`f8159d4`)
- [ ] `make tools` (đang chạy) → `sudo -v && make disk` → `make qemu`
- [ ] Sửa `/home/tmnvi/...` còn sót trong vault theo user mới
- [ ] Push lên GitHub

## Bước cuối: DỌN SẠCH DẤU VẾT

> _User chốt: xong hết thì vault không được nhắc gì tới chuyện mất source / dựng lại nữa._

Khi checklist trên tick hết, làm một lượt rồi commit:

1. **Xoá file này.**
2. Gỡ mọi câu nhắc tới "mất source / dựng lại / máy cũ / viết lại từ note" ở:
   - [[Home]] — callout cảnh báo đầu trang + dòng Meta trỏ note này
   - [[CLAUDE]] — tiêu đề mục "Vị trí ... MÁY MỚI", câu cảnh báo, mục "Trạng thái môi trường",
     dòng backup trong "Quy ước làm việc" (giữ **quy ước backup**, bỏ lý do "sau khi mất sạch source")
   - [[Cẩm nang build & debug]] — callout đầu note
   - [[Lab 0x00 - Hello Serial]], [[Lab 0x01 - Bootloader Parser]], [[Lab 0x02 - Framebuffer]],
     [[Lab 0x03 - PMM & VMM]] — callout đầu note (giữ nguyên phần output thật + mục "Chạy lại")
   - [[Dựng môi trường chung]], [[Dựng môi trường cavOS]], [[Dựng môi trường oskernel-lab]] —
     dòng "Liên quan" và mấy câu kiểu "sau khi mất source"
   - `Thực hành/src/oskernel-lab/README.md` — viết lại thành mô tả cây source bình thường
3. Cân nhắc gộp/viết lại message của commit `f8159d4` cho trung tính (chưa push nên sửa được).

**Giữ lại** (đây là kiến thức thật, không phải chuyện sự cố): output thật trong note Lab, 3 note dựng
môi trường, các bảng "bẫy đã dính", quy ước backup `Thực hành/src/`, `.gitattributes`.
