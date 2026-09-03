---
tags: [meta, build, setup, cavos]
status: đang làm
---

# Dựng môi trường cavOS (máy trắng)

> _Công thức dựng **riêng cavOS**: cài gói, clone repo, đúc cross-compiler, tạo `disk.img`, boot QEMU.
> Đây là repo đọc để học, không phải code mình viết._

Nền chung (WSL, quy tắc build ở đâu, gói dùng chung): [[Dựng môi trường chung]] — làm trước note này.
Liên quan: [[Dựng môi trường oskernel-lab]] · [[Cẩm nang build & debug]]

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
make qemu          # boot disk.img  (máy này KHÔNG chạy được, xem mục 5)
```

`disk.img` ra khoảng **1.9 GB** — nó là ảnh nguyên ổ đĩa chứa cả userland Alpine, không phải mỗi kernel.

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

## 5. Chạy QEMU: máy này không có KVM

> _`make qemu` của cavOS luôn kèm `-enable-kvm`. Trên máy này cờ đó không dùng được, và đây không phải
> lỗi cấu hình — là giới hạn của Windows 10._

Triệu chứng đi theo 2 nấc, đừng nhầm chúng với nhau:

| Lỗi | Nghĩa là |
| --- | --- |
| `Permission denied` | mở `/dev/kvm` không được vì user chưa ở group `kvm`. **Sửa được**: `sudo usermod -aG kvm $USER` rồi `wsl --shutdown`. |
| `No such device` (errno 19) | đã mở được file rồi, nhưng **bên dưới không có KVM**. Không sửa được. |

Vì sao: nested virtualization trong WSL2 chỉ có trên **Windows 11 / Server 2022** trở lên. Máy này là
**Windows 10 Home (19045)** nên `/dev/kvm` chỉ là cái vỏ — thêm `nestedVirtualization=true` vào `.wslconfig`
cũng vô ích. Kiểm nhanh:

```bash
python3 -c "import os; os.open('/dev/kvm', os.O_RDWR)"    # ENODEV = không có KVM thật
```

Cách chạy thay thế — `~/cavos-qemu.sh`, y hệt target `qemu` nhưng bỏ `-enable-kvm`:

```bash
~/cavos-qemu.sh            # cửa sổ SDL
~/cavos-qemu.sh -nogfx     # headless, chỉ serial (tiện chép log)
```

```bash
#!/usr/bin/env bash
set -euo pipefail
cd ~/cavOS/src/kernel
DISPLAY_ARGS=(-vga vmware -display sdl)
[ "${1:-}" = "-nogfx" ] && DISPLAY_ARGS=(-display none)
exec qemu-system-x86_64 -d guest_errors -serial stdio     -drive file=../../disk.img,format=raw,id=disk,if=none     -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0     -m 4g -netdev user,id=mynet0 -net nic,model=e1000,netdev=mynet0     "${DISPLAY_ARGS[@]}"
```

> Không sửa `src/kernel/Makefile` của cavOS: đó là code upstream, sửa vào sẽ vướng khi `git pull`.
> Chậm hơn KVM khá nhiều nhưng boot tới `REACHED SYSTEM` vẫn chỉ mất vài giây.

## 6. Boot thành công trông thế nào

> _Để lần sau boot mà thiếu dòng nào thì biết ngay hỏng ở đâu. Mốc quan trọng nhất là
> `====== REACHED SYSTEM ======` — tới được đó nghĩa là phần khởi tạo lõi đã xong._

Rút gọn từ một lần boot thật (`-m 4g`, không KVM):

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
> đúng cơ chế [[Lab 0x03 - PMM & VMM]] tự dựng. Lý thuyết và thực hành khớp nhau ở đây.
## 7. Bẫy đã dính (đừng dính lại)

| Triệu chứng | Nguyên nhân | Cách tránh |
| --- | --- | --- |
| `x86_64-cavos-gcc was not found!` | chưa `make tools` | chạy `make tools` (1 lần) |
| `Could not access KVM kernel module: Permission denied` | user chưa ở group `kvm` | `sudo usermod -aG kvm $USER` rồi `wsl --shutdown` bên Windows |
| `Could not access KVM kernel module: No such device` | **máy này không có KVM thật** — xem mục dưới | dùng `~/cavos-qemu.sh` (bỏ `-enable-kvm`) |
| `sudo -v` gõ ở terminal khác vẫn bị hỏi lại mật khẩu | Ubuntu đặt `timestamp_type=tty`, cache gắn với đúng terminal đó | gõ `sudo -v` và `make disk` **trong cùng một terminal** |
| `sudo: timed out` giữa `make disk` | bước `ports` hỏi mật khẩu muộn | `sudo -v` ngay trước |
| `tar` thiếu file (vd `opcode/i386.h`) | giải nén trên `/mnt` (9p) | build trong `~`, xem [[Dựng môi trường chung]] |
| `env: bash\r` | script `.sh` bị CRLF do nằm ổ Windows | build trong `~` |
| `Error relocating /bin/bash` | rootfs `target/` copy qua `/mnt` nên hỏng | bootstrap sạch trong `~` |

