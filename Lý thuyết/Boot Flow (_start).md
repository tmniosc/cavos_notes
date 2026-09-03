---
tags: [moc, boot]
---

# 🧭 Boot Flow — `_start()`

Nguồn: `~/cavOS/src/kernel/entry/kernel.c :: _start()`.
Hàm này gọi các bước khởi tạo **đúng theo thứ tự phụ thuộc phần cứng** → dùng làm giáo trình.

```
[Nguồn] → Firmware (UEFI/BIOS) → Bootloader ([[Limine Protocol|Limine]]) → Kernel (_start)
```

| #   | Bước trong `_start()`               | File code                       | Note                                   | Spec                     |
| --- | ----------------------------------- | ------------------------------- | -------------------------------------- | ------------------------ |
| 0   | `_start`, `LIMINE_BASE_REVISION(2)` | `entry/kernel.c`, `link.ld`     | [[Step 00 - Boot & Limine]]            | [[Limine Protocol]]      |
| 1   | `initiateSerial()`                  | `drivers/serial.c`              | [[Step 01 - Serial UART]]              | [[UART 16550]]           |
| 2   | `initialiseBootloaderParser()`      | `entry/bootloader.c`            | [[Step 02 - Bootloader Parser]]        | [[Limine Protocol]]      |
| 3   | `initiateVGA/Console()`             | `graphical/`                    | [[Step 03 - Framebuffer & Console]]    | Limine FB, PSF           |
| 4   | `initiatePMM()`                     | `memory/pmm.c`                  | [[Step 04 - Physical Memory Manager]]  | [[Intel SDM]] §4         |
| 5   | `initiateVMM()`/`initiatePaging()`  | `memory/vmm.c`,`paging.c`       | [[Step 05 - Virtual Memory & Paging]]  | [[Intel SDM]] Vol3 §4    |
| 6   | `initiateGDT()`                     | `cpu/gdt.c`                     | [[Step 06 - GDT & TSS]]                | [[Intel SDM]] Vol3 §3,§8 |
| 7   | `initiateACPI()`                    | `acpi/`                         | [[Step 07 - ACPI]]                     | ACPI Spec                |
| 8   | `initiateISR()`                     | `cpu/idt.c,isr.*`               | [[Step 08 - IDT & Interrupts]]         | [[Intel SDM]] Vol3 §6    |
| 9   | `initiateApicTimer()`               | `cpu/apic.c,timer.c`            | [[Step 09 - APIC & Timer]]             | [[Intel SDM]] Vol3 §11   |
| 10  | `initiateKb/Mouse()`                | `drivers/`                      | [[Step 10 - PS2 Keyboard & Mouse]]     | 8042 datasheet           |
| 11  | `initiateTasks()` + sched           | `multitasking/`                 | [[Step 11 - Multitasking & Scheduler]] | [[Intel SDM]] Vol3 §8    |
| 12  | `initiateNetworking()`              | `networking/`                   | [[Step 12 - Networking]]               | lwIP, RFC                |
| 13  | `initiatePCI()`                     | `drivers/`                      | [[Step 13 - PCI & NIC]]                | PCI Spec                 |
| 14  | `fsMount` AHCI                      | `filesystems/`,`drivers/`       | [[Step 14 - AHCI & Filesystems]]       | AHCI Spec                |
| 15  | `initiateSyscall*()`                | `cpu/fastSyscall.c`,`syscalls/` | [[Step 15 - Fast Syscalls]]            | [[Intel SDM]] Vol2       |
| 16  | `initiateSSE()`                     | `cpu/sse.asm`                   | [[Step 16 - SSE & FPU]]                | [[Intel SDM]] Vol1       |
| 17  | `run("/bin/bash")`                  | `entry/`, `multitasking/`       | [[Step 17 - Userspace & ELF Loader]]   | ELF64 ABI                |

## Trạng thái CPU khi `_start` chạy
Limine đã lo sẵn (xem [[Limine Protocol]] mục 5): [[Long Mode]] bật, paging bật, [[HHDM]] đã map,
kernel ở [[Higher-Half Kernel|higher-half]], stack sẵn sàng, các `response` đã điền.
