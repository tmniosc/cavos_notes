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
├─ 07-tasks/             task struct + danh sách task như cavOS, kernel thread có PML4 + stack riêng, schedule() đổi task mỗi tick bằng khung iretq, handControl (page fault ma thuật), sleep/BLOCKED, idle hlt, reaper; scripts/tasks_input.py
├─ 08-pci-nic/           quét PCI qua 0xCF8/0xCFC như initiatePCI() + đo cỡ BAR, driver e1000 (bus master, BAR0 MMIO, reset, MAC từ EEPROM, ring RX/TX 256 descriptor, IRQ level qua I/O APIC dò GSI), netQueue -> helperNet; DHCP + ARP + ping tới slirp 10.0.2.2, filter-dump pcap; scripts/pcap_summary.py
├─ 09-ahci-fs/          AHCI như initiateAHCI() (reset HBA, BOHC, AE, dò cổng, rebase, READ DMA EXT + PRDT mỗi trang), MBR, fsMount / và /boot/, FAT32 (chuỗi cluster, LFN) + ext2 (inode, block gián tiếp) chỉ đọc; scripts/mkimage_fs.sh dựng ổ FAT32 + ext2, scripts/run_all.sh
├─ 0a-fast-syscalls/    SYSCALL/SYSRET như cavOS (initiateSyscallInst, syscall_entry chép nguyên, syscallHandler + bảng), task ring 3 từ asm (user.S), stack syscall mỗi task, int 0x80; DEMO=-DINT80_STI, -DBAD_SYSRET; scripts/run_all.sh
├─ 0b-sse-fpu/          initiateSSE() như cavOS (CR0/CR4/XCR0), vùng FPU mỗi task, fxsave/fxrstor trong schedule(); 2 task ring 3 tranh YMM0/ST(0); DEMO=-DXSAVE, -DNO_FPU_SAVE; QEMU -cpu max; scripts/run_all.sh
└─ 0c-elf-userspace/    nạp ELF như elfExecute() (đọc từ ext2, PML4 mới, PT_LOAD + xoá .bss) và stack như stackGenerateUser() (argc/argv/envp/auxv); user/hello.c build thành /bin/hello, in lại những gì nhận được; scripts/run_all.sh
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
