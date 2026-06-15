---
tags: [reference, build]
---

# Build & Debug Cheatsheet

Repo: `~/cavOS` (trong WSL native fs — **không** dùng `/mnt/d`).

## Build & chạy
```bash
cd ~/cavOS
make tools          # build cross-compiler x86_64-cavos-gcc (1 LẦN duy nhất, lâu)
make disk           # build disk.img (kernel + bootloader + userland). Cần sudo cho bước ports
make qemu           # boot disk.img trong QEMU
make kernel         # chỉ build kernel.bin
make clean          # dọn .o
```
> Mẹo: chạy `sudo -v` ngay trước `make disk` để sudo không timeout ở bước `ports` (bootstrap Alpine).

## Debug (GDB + QEMU)
```bash
make qemu_dbg       # QEMU mở gdb stub (thường cổng :1234, dừng chờ)
make dev            # clean + build_dirty + qemu_dbg
```
GDB kết nối:
```bash
gdb src/kernel/kernel.bin
(gdb) target remote :1234
(gdb) break _start
(gdb) continue
```

## Output debug serial
`debugf()` ([[Step 01 - Serial UART]]) in ra COM1 → QEMU hiển thị qua cờ `-serial` (xem `src/kernel/Makefile` mục `qemu`).

## Xem code bằng Zed + clangd (LSP)
> Đọc/điều hướng code (jump-to-def, find-refs, hover type) dễ hơn nhiều khi clangd hiểu đúng flag kernel.
> Kernel freestanding dùng flag đặc thù (`-ffreestanding -mcmodel=kernel -I<custom>`...) → **bắt buộc** có
> `compile_commands.json`, không thì clangd báo đỏ "not found" khắp nơi.

**Cài 1 lần (cần sudo):**
```bash
sudo apt-get install -y bear clangd
```
**Sinh `compile_commands.json`** (mỗi khi đổi cấu trúc build / thêm file) — `common.mk` có target sẵn:
```bash
cd ~/oskernel-lab/03-pmm-vmm
make compdb         # = bear --output compile_commands.json -- make -B $(OBJS)
```
`make clean` GIỮ lại `compile_commands.json` (để LSP còn chạy); `make distclean` mới xóa nó.
> Lưu ý: `clangd --check=foo.c` có thể báo "N errors" dạng `tweak: SwapBinaryOperands FAIL` tại các macro —
> **đó KHÔNG phải lỗi code**, chỉ là clangd tự thử refactor nội bộ. Lỗi thật là dòng `error:` (grep nó → rỗng = OK).
**`.clangd`** đặt ở gốc `~/oskernel-lab` (đã tạo) — gỡ vài flag GCC-only Clang không nuốt
(`-mcmodel=kernel`, `-mno-red-zone`, `-fno-stack-clash-protection`) + tắt cảnh báo include nhiễu.
Kiểm nhanh clangd hiểu đúng chưa: `clangd --check=pmm.c` (thấy "Indexing AST" + không lỗi include = OK;
dòng `tweak: ... FAIL` chỉ là refactor nội bộ, bỏ qua).

**Mở Zed:** code nằm trong WSL ext4. Sạch nhất là **cài Zed bản Linux trong WSL** (`zed ~/oskernel-lab`) để
clangd chạy native + path khớp `compile_commands.json` (đường dẫn Linux `/home/tmnvi/...`). Mở qua
`\\wsl.localhost\...` bằng Zed Windows cũng được nhưng path Win/Linux dễ lẫn → hay lỗi.
> Cho **cavOS thật**: `cd ~/cavOS && make clean && bear -- make kernel` (chỉ phần kernel, nhanh; `make disk`
> lâu + cần sudo). `compile_commands.json` sinh ra ở gốc repo, Zed đọc được ngay.

## Lab kernel (`~/oskernel-lab`) — build artifacts tự sinh
Các mini-project lab dùng chung `common.mk` (mỗi project chỉ khai `SRCS/KERNEL/IMG` rồi `include ../common.mk`).
```bash
cd ~/oskernel-lab/01-bootloader-parser
make            # build kernel.bin + 4 artifact (.map .dis .sym .elf.txt)
make clean      # xoá .o, kernel.bin, 4 artifact, os.img
make image      # tạo os.img bootable (cần mtools; mkimage.sh)
make run        # boot QEMU (-serial stdio, headless)
```
Mỗi lần build kernel tự sinh kèm (tên dẫn xuất từ `KERNEL`, vd `kernel.bin` → `kernel.*`):

| File | Lệnh sinh | Dùng để |
|---|---|---|
| `kernel.map` | linker `-Wl,-Map=` (ngay trong bước link) | layout section/symbol + địa chỉ load |
| `kernel.dis` | `x86_64-cavos-objdump -d -M intel` | đối chiếu code ↔ asm khi học/debug |
| `kernel.sym` | `x86_64-cavos-nm -n` (sort theo addr) | tra địa chỉ hàm/biến |
| `kernel.elf.txt` | `x86_64-cavos-readelf -a` | program/section headers mà Limine nạp |

> ⚠️ **`kernel.map` là sản phẩm của LINKER (`kernel.bin`), KHÔNG phải của `kernel.o`.** `.o` chưa link
> nên chưa có địa chỉ cuối để "map". `kernel.bin` link từ **một mình `kernel.o`** + `linker.ld`, kiểu
> **freestanding** (`-nostdlib -static -no-pie`): không libc, không crt0 — Limine thay crt nhảy thẳng `kmain`.

> Cross tools (`objdump/readelf/nm`) không nằm trên `$PATH` → `common.mk` gọi full path `/home/tmnvi/opt/cross/bin/x86_64-cavos-*`.
> ⚠️ Trong Makefile, **đừng** để comment cùng dòng với `VAR := value` — Make nuốt cả khoảng trắng trước `#` vào giá trị → tên file dính space, hỏng build.

> 💡 Soi section/flag của **`.o`** (trước link): `readelf -S kernel.o` / `objdump -h kernel.o`
> → thấy `.text`=AX, `.rodata`=A, `.data`/`.bss`=WA (gốc của W^X — xem [[Step 00 - Boot & Limine]] §0.1.2).

## Cấu trúc `os.img` (ảnh đĩa boot — MBR + FAT32 + Limine)
`os.img` **không phải** kernel — nó là **ảnh nguyên ổ đĩa** (lab: 64 MiB), QEMU coi như HDD.
Dựng bằng `scripts/mkimage.sh` (mtools, **không cần sudo/mount** — hợp WSL2).

```
os.img  (64 MiB = 131072 sector × 512B)
┌───────────────────────────────────────────────────────────┐
│ sector 0       : MBR — bảng phân vùng + chữ ký 55 AA      │
│ sector 1..2047 : trống 1 MiB — Limine BIOS stage chen vào │
├───────────────────────────────────────────────────────────┤
│ sector 2048..  : PHÂN VÙNG 1 = FAT32, cờ boot ✔ (63 MiB)  │  ← bắt đầu @1 MiB
│   ::/boot/kernel.bin            ← KERNEL của bạn          │
│   ::/boot/limine/limine.conf    ← menu boot (kernel_path) │
│   ::/boot/limine/limine-bios.sys← Limine khi boot BIOS    │
│   ::/EFI/BOOT/BOOTX64.EFI       ← Limine khi boot UEFI64  │
│   ::/EFI/BOOT/BOOTIA32.EFI      ← Limine khi boot UEFI32  │
└───────────────────────────────────────────────────────────┘
```

- **Phân vùng bắt đầu @1 MiB (sector 2048)**: chừa chỗ cho Limine BIOS stage → mtools thao tác tại
  `os.img@@1M`. `limine bios-install` ghi MBR + vùng trống này.
- **Hỗ trợ cả 2 đường boot**: BIOS (MBR → `limine-bios.sys`) **và** UEFI (firmware tự tìm
  `/EFI/BOOT/BOOTX64.EFI`). Cả hai → đọc `limine.conf` → nạp `kernel.bin` → `kmain`.
- `limine.conf` (lab): `timeout: 0` (boot ngay), `kernel_path: boot():/boot/kernel.bin`,
  `protocol: limine` → nối [[Step 00 - Boot & Limine]] §0.1.1 (Limine ELF loader đọc `kernel.bin`).

### Mạch source → chạy
```
kernel.c ─gcc─► kernel.o ─ld(-T linker.ld)─► kernel.bin ─┐
limine.conf + limine-bios.sys + BOOTX64.EFI ─────────────┤
                                                          ▼
              mkimage.sh: dd → parted(MBR) → limine bios-install
                        → mformat FAT32 → mcopy file vào
                                                          ▼
                                       os.img ─qemu─► Limine ─► kmain
```

### Soi nhanh os.img
```bash
fdisk -l os.img                 # bảng phân vùng
mdir -i "os.img@@1M" -b -/ ::   # cây file trong FAT32
xxd -s 510 -l 2 os.img          # chữ ký boot 55 AA
```

## Lỗi thường gặp (đã trải qua)
- `env: bash\r` → CRLF (do build trên ổ Windows). Build trong `~`, git Linux giữ LF.
- `tar` thiếu file (`opcode/i386.h`) → giải nén trên `/mnt/` (9p). Build trong `~` (ext4).
- `Error relocating /bin/bash` → rootfs `target/` hỏng do copy qua `/mnt/`. Clone sạch.
- `sudo: timed out` → bước ports cần mật khẩu; chạy `sudo -v` trước.
