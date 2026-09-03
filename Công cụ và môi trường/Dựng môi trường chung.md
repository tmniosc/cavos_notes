---
tags: [meta, build, setup]
status: đang làm
---

# Dựng môi trường chung (nền cho cả cavOS lẫn lab)

> _Phần **dùng chung** cho mọi thứ trong vault này: WSL nằm ở đâu, vault nằm ở đâu, gói nào cả hai nhánh
> đều cần, và quy tắc vàng "build ở đâu". Làm xong note này rồi mới rẽ nhánh._

Rẽ nhánh: [[Dựng môi trường cavOS]] (đọc code người ta) · [[Dựng môi trường oskernel-lab]] (code tự viết)
Liên quan: [[Cẩm nang build & debug]] · [[Home]]

## Nói thật đơn giản

> _Trước khi nấu món gì thì phải có **căn bếp** đã: chỗ đứng, điện nước, và mấy dụng cụ ai nấu món nào
> cũng phải dùng. Note này là căn bếp; hai note kia là hai món ăn._

Ba thứ cần nắm:

1. **Chỗ nấu**: Linux thật (WSL) chứ không phải Windows. Và phải nấu **bên trong nhà** (`~`), không bê ra
   sân (`/mnt/d` — ổ Windows). Bê ra sân là hỏng, đã hỏng vài lần rồi, lý do ở dưới.
2. **Chỗ ghi chép**: vault để bên Windows (ổ D) cho tiện mở Obsidian + đẩy GitHub. Code thì ở trong WSL.
   Hai chỗ khác nhau, đừng lẫn.
3. **Dụng cụ chung**: vài gói `apt` mà cả cavOS lẫn lab đều cần. Cài một lần, dùng cho cả hai.

## Bản đồ: cái gì nằm đâu

| Thứ | Đường dẫn | Ghi chú |
| --- | --- | --- |
| WSL distro | `Ubuntu-26.04` (WSL2) | `wsl.exe -l -v` để xem |
| user / home | `tmniosc` → `/home/tmniosc` | |
| **Vault** (note) | Windows `D:\tmniosc\cavos_notes` | trong WSL: `/mnt/d/tmniosc/cavos_notes` |
| Vault trên GitHub | `github.com/tmniosc/cavos_notes` | remote `origin` |
| **Source cavOS** | `~/cavOS` | [[Dựng môi trường cavOS]] |
| **Source lab** | `~/oskernel-lab` | [[Dựng môi trường oskernel-lab]] |
| Bản lưu source lab | vault `Thực hành/src/oskernel-lab/` | chỉ để backup + push, **không build ở đó** |
| Cross toolchain | `~/opt/cross/bin/x86_64-cavos-gcc` | chỉ cavOS bắt buộc |
| Limine binary | `~/opt/limine` | chỉ lab dùng |

Claude chạy phía **Windows**, gọi vào WSL qua `wsl.exe`; xem file WSL từ Windows bằng UNC
`\\wsl.localhost\Ubuntu-26.04\home\tmniosc\...`.

> `sudo` trên máy này **hỏi mật khẩu** → mọi lệnh `sudo` phải tự gõ tay.

## Quy tắc vàng: build TRONG `~`, không bao giờ trên `/mnt`

> _Đây là quy tắc quan trọng nhất cả vault. Vi phạm là hỏng build theo kiểu rất khó đoán ra nguyên nhân._

`/mnt/c`, `/mnt/d` là ổ Windows được WSL "giả lập" qua giao thức **9p**, còn `~` là ext4 thật của Linux.
Ba kiểu hỏng đã gặp:

| Hỏng | Vì sao |
| --- | --- |
| `tar` giải nén **thiếu file** (vd `opcode/i386.h`) rồi build lỗi giữa chừng | 9p không đáng tin với thao tác file dày đặc |
| Script `.sh` báo `env: bash\r` | ổ Windows + `core.autocrlf=true` → file thành CRLF, Linux đọc `\r` như một phần tên chương trình |
| `Error relocating /bin/bash` | rootfs (`target/`) copy qua `/mnt` bị hỏng quyền/symlink |

Hệ quả cho cách làm việc:

- **Đọc/sửa code + build**: trong WSL (`~`).
- **Ghi note**: vault bên D:.
- **Chép qua `/mnt` chỉ 1 chiều, chỉ file text nhỏ** (vault → WSL). Không giải nén, không build ở đó.
- Vault có `.gitattributes` ép `Thực hành/src/**` luôn **LF** để lần chép ngược không dính CRLF.

## Gói dùng chung

> _Mấy gói này nhánh nào cũng cần. Gói riêng của từng nhánh nằm trong note tương ứng._

```bash
sudo apt update && sudo apt install -y build-essential git curl parted qemu-system-x86
```

| Gói | Để làm gì |
| --- | --- |
| `build-essential` | gcc + make + binutils của hệ thống |
| `git` | clone cavOS / Limine, và commit vault |
| `parted` | tạo bảng phân vùng cho ảnh đĩa (cả `disk.img` lẫn `os.img`) |
| `qemu-system-x86` | boot thử ảnh đĩa |
| `curl` | tải lặt vặt |

## Kiểm tra nhanh môi trường

```bash
wsl.exe -l -v                                  # chạy từ Windows: distro + trạng thái
whoami; echo $HOME; df -h /                    # user, home, dung lượng ext4
for t in gcc make git parted qemu-system-x86_64 nasm mformat; do \
  printf "%-20s %s\n" "$t" "$(command -v $t || echo MISSING)"; done
ls ~/cavOS ~/oskernel-lab ~/opt 2>/dev/null    # đã dựng tới đâu
```

## Rẽ nhánh

```
môi trường chung (note này)
        │
        ├─► cavOS  : + gói build cross-compiler ─► make tools ─► make disk ─► make qemu
        │            [[Dựng môi trường cavOS]]
        │
        └─► lab    : + mtools + Limine v8.x ────► cp Thực hành/src ─► make run
                     [[Dựng môi trường oskernel-lab]]
```

Hai nhánh **độc lập**: lab chạy được ngay cả khi cavOS chưa `make tools` xong, và ngược lại.

## Tiến độ trên máy này

- [x] WSL `Ubuntu-26.04`, user `tmniosc`
- [x] Gói dùng chung đã cài
- [x] Vault ở `D:\tmniosc\cavos_notes`, đã có remote GitHub
