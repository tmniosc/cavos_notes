---
tags: [meta, build, setup, cavos]
status: đang làm
---

# Dựng môi trường cavOS (máy trắng)

> _Công thức dựng **riêng cavOS**: cài gói, clone repo, đúc cross-compiler, tạo `disk.img`, boot QEMU.
> Đây là repo đọc để học, không phải code mình viết._

Nền chung (WSL, quy tắc build ở đâu, gói dùng chung): [[Dựng môi trường chung]] — làm trước note này.
Liên quan: [[Dựng môi trường oskernel-lab]] · [[Cẩm nang build & debug]] · [[Khôi phục source (2026-09-03)]]

## Nói thật đơn giản

> _Nấu được món này thì cần một **cái máy dịch tự chế**, và đó là chỗ tốn thời gian nhất._

- gcc bình thường dịch chương trình cho *Linux đang chạy*: nó mặc định có sẵn thư viện chuẩn, có hệ điều
  hành đỡ lưng. Kernel thì **chạy một mình trên phần cứng trần** — không libc, không ai đỡ.
- Nên phải tự đúc một gcc riêng tên `x86_64-cavos-gcc`: dịch ra mã cho "hệ điều hành cavOS", không phải
  cho Linux. Đúc lâu (tải nguồn binutils + GCC rồi build) nhưng **chỉ đúc một lần**.
- Đúc xong mới `make disk`: gói kernel + bootloader + phần mềm thành một file `disk.img` (ổ cứng giả),
  rồi `make qemu` bật máy ảo lên chạy.

Thứ tự bắt buộc: **gói → máy dịch → disk.img**. Thiếu bước nào thì bước sau chặn ngay.

## 0. Máy hiện tại

| Thứ | Giá trị |
| --- | --- |
| Repo cavOS | `~/cavOS` |
| Cross toolchain | `~/opt/cross/bin/x86_64-cavos-gcc` |
| Nguồn | `github.com/malwarepad/cavOS`, branch `master` |

## 1. Gói riêng của cavOS

> _Ngoài gói chung, phần lớn danh sách này tồn tại **chỉ để build được cross-compiler** — chính
> `docs/install.md` của cavOS cũng nói vậy._

```bash
sudo apt install -y \
  nasm dosfstools bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-dev \
  autoconf automake wget
```

| Nhóm | Để làm gì |
| --- | --- |
| `bison flex libgmp3-dev libmpc-dev libmpfr-dev libisl-dev texinfo autoconf automake wget` | **đúc cross-compiler** — binutils + GCC cần mấy thư viện số học này |
| `nasm` | kernel cavOS có vài file assembly |
| `dosfstools` | `mkdosfs` tạo FAT32 + `fatlabel` gắn nhãn `CAVOS` cho `disk.img` |

## 2. Clone repo

```bash
git clone https://github.com/malwarepad/cavOS.git ~/cavOS
```

- **Bắt buộc để trong `~`** (ext4 native), lý do ở [[Dựng môi trường chung]].
- Repo công khai của người khác → mất lúc nào cũng clone lại được, **không có gì để backup**.
  Kiến thức nằm ở vault này, không nằm trong repo. (Ngược hẳn với lab — xem [[Dựng môi trường oskernel-lab]].)

## 3. Cross-compiler: `make tools` (1 LẦN duy nhất, LÂU)

```bash
cd ~/cavOS && make tools
```

Sinh ra `~/opt/cross/bin/x86_64-cavos-gcc` (kèm `objdump/readelf/nm/ld` cùng tiền tố).
Script tự tải nguồn binutils + GCC rồi build → **rất lâu**, phụ thuộc tốc độ mạng.

> Đo thật trên máy này: **GCC 11.4.0**, log build ~170k dòng. Thứ tự nó làm:
> binutils → GMP → MPFR → MPC → ISL → GCC. Đáng chú ý là nó **tự build GMP/MPFR/MPC/ISL từ source**
> chứ không dùng `libgmp3-dev`... đã cài qua apt — nên phần lớn gói `-dev` ở mục 1 gần như không được
> đụng tới, và thời gian lâu hơn mình tưởng.

`make disk` gọi target `verifytools` để kiểm tra file này tồn tại; thiếu là dừng ngay với dòng đỏ
*"x86_64-cavos-gcc was not found!"*. `Makefile` còn so **ngày sửa** của file với mốc `GCC_CHECK_DATE`
nên toolchain quá cũ cũng bị bắt build lại.

> Cross tools **không nằm trên `$PATH`** → gọi bằng đường dẫn đầy đủ.

## 4. Build disk + chạy

> _`make disk` gói mọi thứ thành ảnh đĩa; bước `ports` bootstrap Alpine nên cần quyền sudo._

```bash
sudo -v            # làm mới sudo TRƯỚC, kẻo giữa chừng "sudo: timed out"
cd ~/cavOS
make disk          # limine + uacpi + musl + ports + kernel -> disk.img
make qemu          # boot disk.img
```

Chuỗi phụ thuộc trong `Makefile` gốc:

```
disk ─► disk_prepare ─► verifytools ─► limine ─► uacpi ─► musl ─► ports
                                                                    │
                                        src/software/{test,badtest,drawimg}
                                                                    │
                                                    ─► make -C src/kernel disk ─► disk.img
```

Vòng lặp hằng ngày về sau chỉ còn `make disk && make qemu` — xem [[Cẩm nang build & debug]] cho
`make kernel`, `make qemu_dbg`, GDB và `compile_commands.json`.

## 5. Bẫy đã dính (đừng dính lại)

| Triệu chứng | Nguyên nhân | Cách tránh |
| --- | --- | --- |
| `x86_64-cavos-gcc was not found!` | chưa `make tools` | chạy `make tools` (1 lần) |
| `Could not access KVM kernel module: Permission denied` | user chưa ở group `kvm` (WSL có sẵn `/dev/kvm` nhưng quyền `root:kvm`) | `sudo usermod -aG kvm $USER` rồi `wsl --shutdown` bên Windows. Chưa muốn sửa thì chạy tay lệnh qemu **bỏ `-enable-kvm`** (chậm hơn nhưng boot được) |
| `sudo -v` gõ ở terminal khác vẫn bị hỏi lại mật khẩu | Ubuntu đặt `timestamp_type=tty`, cache gắn với đúng terminal đó | gõ `sudo -v` và `make disk` **trong cùng một terminal** |
| `sudo: timed out` giữa `make disk` | bước `ports` hỏi mật khẩu muộn | `sudo -v` ngay trước |
| `tar` thiếu file (vd `opcode/i386.h`) | giải nén trên `/mnt` (9p) | build trong `~`, xem [[Dựng môi trường chung]] |
| `env: bash\r` | script `.sh` bị CRLF do nằm ổ Windows | build trong `~` |
| `Error relocating /bin/bash` | rootfs `target/` copy qua `/mnt` nên hỏng | bootstrap sạch trong `~` |

## Tiến độ trên máy này

- [x] Gói riêng đã cài
- [x] `git clone cavOS` → `~/cavOS`
- [x] `make tools` → `x86_64-cavos-gcc (GCC) 11.4.0`
- [x] `make disk` → `disk.img` 1.88 GB
- [x] Boot QEMU: chạm `====== REACHED SYSTEM ======` (chạy không KVM, xem bảng bẫy)

Lần boot đầu tiên trên máy mới in ra (rút gọn):

```
[serial]   Installing serial...
[graphics] Resolution fixed: fb{ffff8000fd000000} dim(xy){1024x768} bpp{32}
[console]  Initiated with font: dim(xy){8x14}
[pmm]      Bitmap initiated: bitmapStartPhys{0x60000} size{20050}
[acpi::info] starting uACPI, version 3.1.0   ... 54 devices, 0 thermal zones
[apic]     Detection completed: lapic{fee00000} ioapic{fec00000}
====== REACHED SYSTEM ======
[pci::e1000] Intel E1000 NIC detected! dev{100e}
[pci::ahci]  Detected controller! name{Intel ICH9} ... SATA drive found at port 0
[syscalls] System calls are ready to fire: 99/450
```

> Dòng `fb{ffff8000fd000000}` chính là framebuffer nhìn qua HHDM — **cùng địa chỉ** mà
> [[Lab 0x02 - Framebuffer]] in ra. Còn `[pmm] bitmapStartPhys{0x60000}` là bitmap PMM của cavOS thật,
> đúng kiểu [[Lab 0x03 - PMM & VMM]] dựng lại. Lý thuyết và thực hành khớp nhau ở đây.
