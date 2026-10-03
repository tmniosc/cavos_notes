# oskernel-lab — source lab kernel x86_64

Kernel tự viết từ số 0, học song song với việc đọc cavOS. Note tương ứng nằm trong vault:
`Thực hành/Lab 0x00` → `Lab 0x03`.

Đây là **bản lưu** trong vault để backup + push GitHub. **Build thì làm trong WSL** (`~/oskernel-lab`),
không build trực tiếp ở đây (ổ Windows qua 9p làm hỏng build, và CRLF làm chết script `.sh`).

```
oskernel-lab/
├─ common.mk          build chung: tự dò cross x86_64-cavos-gcc, không có thì dùng gcc hệ thống
├─ linker.ld          bản gốc; mỗi project giữ 1 copy
├─ limine.conf        timeout 0, protocol limine, kernel_path boot():/boot/kernel.bin
├─ limine.h           lấy từ cavOS (src/kernel/include/limine.h) — base revision 2
├─ .clangd            gỡ vài flag GCC-only cho clangd
├─ .gitattributes     ép toàn bộ file về LF
├─ scripts/mkimage.sh dựng os.img: MBR + FAT32@1MiB + limine bios-install (mtools, không cần sudo)
├─ 00-hello-serial/      boot Limine + UART 16550
├─ 01-bootloader-parser/ đọc 4 Limine request: paging/HHDM/kernel address/memmap
├─ 02-framebuffer/       vẽ pixel + chữ, font 8x8 nhúng
├─ 03-pmm-vmm/           bitmap PMM + tự dựng PML4, mov cr3, vmap/vresolve
├─ 04-gdt-idt/           GDT + TSS như cavOS (lgdt, lretq, ltr), IDT 256 cổng + stub ISR, bắt #BP/#DE/#UD/#GP/#PF, IST1 cho #DF
├─ 05-acpi-apic/         RSDP → RSDT/XSDT → MADT/FADT tự parse, LAPIC (MMIO uncached) + I/O APIC, PIT đo LAPIC timer, periodic 1 ms, sti
├─ 06-ps2/               8042 + bàn phím IRQ1 (set 1 qua translate, keymap Shift/Caps) + chuột IRQ12 (gói 3 byte), ring buffer kiểu /dev/input/eventN; scripts/ps2_input.py gõ phím thật qua QEMU monitor
└─ 07-tasks/             task struct + danh sách task như cavOS, kernel thread có PML4 + stack riêng, schedule() đổi task mỗi tick bằng khung iretq, handControl (page fault ma thuật), sleep/BLOCKED, idle hlt, reaper; scripts/tasks_input.py
```

## Chạy

```bash
cd ~/oskernel-lab/03-pmm-vmm
make info      # đang dùng cross gcc hay gcc hệ thống
make           # kernel.bin + kernel.map/.dis/.sym/.elf.txt
make image     # os.img
make run       # QEMU headless, serial ra stdout
make run-gfx   # có cửa sổ (dùng cho Lab 0x02)
```

Cần `qemu-system-x86`, `mtools`, `parted`, và Limine ở `~/opt/limine`:

```bash
git clone -b v8.x-binary --depth 1 https://github.com/limine-bootloader/limine ~/opt/limine
make -C ~/opt/limine CC=gcc
```

**Phải là Limine v8.x** — từ v9, `kernel_path` đổi tên và `LIMINE_KERNEL_ADDRESS_REQUEST` bị thay bằng
`EXECUTABLE_ADDRESS`, lệch với `limine.h` đang dùng (base revision 2).

Biến chỉnh được từ dòng lệnh, không cần sửa file:

```bash
make run LIMINE_DIR=/duong/dan/khac
make run QEMU_FLAGS="-M q35 -m 512M -serial stdio"
make CROSS_PREFIX=$HOME/opt/cross/bin/x86_64-cavos-
```

Chi tiết dựng máy: note `Dựng môi trường oskernel-lab` trong vault.
