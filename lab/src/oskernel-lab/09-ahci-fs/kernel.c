/* Lab 0x09 — AHCI & Filesystems (Bài 15).
 * Thứ tự giống cavOS _start():
 *   ... initiateTasks -> initiateKernelThreads -> initiateNetworking
 *   -> initiatePCI()                 (quét bus; SATA AHCI -> initiateAHCI)
 *   -> fsMount("/", CONNECTOR_AHCI, 0, 1)      partition 2 của ổ: ext2
 *   -> fsMount("/boot/", CONNECTOR_AHCI, 0, 0) partition 1 của ổ: FAT32 (Limine + kernel)
 * Rồi: liệt kê thư mục, đọc file từ cả hai, so checksum với giá trị script tính trên máy host,
 * và thử PRDT với buffer 2 trang không liền nhau về vật lý.
 * DEMO: -DPRDT_CAVOS (một entry PRDT cho cả buffer, như cavOS ahciSetUpCmd).
 * Chạy: make image (scripts/mkimage_fs.sh dựng ổ MBR + FAT32 + ext2), make run.
 */
#include "../limine.h"
#include "boot.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "acpi.h"
#include "apic.h"
#include "timer.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "serial.h"
#include "task.h"
#include "kernel_helper.h"
#include "pci.h"
#include "nic.h"
#include "ahci.h"
#include "vfs.h"

LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static uint32_t fnv1a(const uint8_t *p, uint64_t n) {
    uint32_t h = 0x811c9dc5u;
    for (uint64_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static uint8_t fileBuf[3 * 1024 * 1024] __attribute__((aligned(4096)));

static void readAndSum(const char *path) {
    uint64_t t0 = timerTicks, c0 = ahciCommands, s0 = ahciSectorsRead;
    long n = fsReadFile(path, fileBuf, sizeof(fileBuf));
    if (n < 0) {
        serial_puts("[kernel] ");
        serial_puts(path);
        serial_puts(": not found\n");
        return;
    }
    serial_puts("[kernel] ");
    serial_puts(path);
    serial_puts(": ");
    serial_putdec(n);
    serial_puts(" bytes, FNV-1a ");
    serial_puthex_short(fnv1a(fileBuf, n));
    serial_puts(", ");
    serial_putdec(ahciCommands - c0);
    serial_puts(" AHCI commands, ");
    serial_putdec(ahciSectorsRead - s0);
    serial_puts(" sectors, ");
    serial_putdec(timerTicks - t0);
    serial_puts(" ms\n");
}

static void printText(const char *path) {
    long n = fsReadFile(path, fileBuf, sizeof(fileBuf) - 1);
    if (n < 0) return;
    fileBuf[n] = 0;
    serial_puts("---------- ");
    serial_puts(path);
    serial_puts(" ----------\n");
    serial_puts((const char *)fileBuf);
    if (n && fileBuf[n - 1] != '\n') serial_putc('\n');
    serial_puts("----------\n");
}

static void whichMount(const char *path) {
    MountPoint *m = fsDetermineMountPoint(path);
    serial_puts("[vfs] fsDetermineMountPoint(\"");
    serial_puts(path);
    serial_puts("\") -> \"");
    serial_puts(m ? m->prefix : "(none)");
    serial_puts("\"\n");
}

/* Buffer 8 KiB = 2 trang VA liền nhau, nhưng 2 frame vật lý KHÔNG liền nhau (chen 1 frame ở giữa).
 * Đọc 16 sector đầu ổ vào đó, so với bản đọc vào buffer liền. */
static void prdtTest(void) {
    static uint8_t ref[8192] __attribute__((aligned(4096)));
    uint64_t f1 = pmm_alloc(), gap = pmm_alloc(), f2 = pmm_alloc();
    uint64_t va = 0xFFFFFE8000800000ULL;
    vmap(va, f1, PTE_RW, 0);
    vmap(va + 4096, f2, PTE_RW, 0);
    memset(P2V(f1), 0, 4096);
    memset(P2V(gap), 0, 4096);
    memset(P2V(f2), 0, 4096);
    getDiskBytes(ref, 0, 16);
    getDiskBytes((uint8_t *)va, 0, 16);
    serial_puts("[prdt] buffer VA ");
    serial_puthex_short(va);
    serial_puts(": page 0 -> PA ");
    serial_puthex_short(f1);
    serial_puts(", page 1 -> PA ");
    serial_puthex_short(f2);
    serial_puts(" (frame ");
    serial_puthex_short(gap);
    serial_puts(" in between)\n");
#ifdef PRDT_CAVOS
    serial_puts("[prdt] mode: PRDT_CAVOS, one PRDT entry = PA of page 0, 8192 bytes\n");
#else
    serial_puts("[prdt] mode: one PRDT entry per page, each with its own PA\n");
#endif
    int ok0 = 1, ok1 = 1, inGap = 0;
    for (int i = 0; i < 4096; i++) {
        if (((uint8_t *)va)[i] != ref[i]) ok0 = 0;
        if (((uint8_t *)va)[4096 + i] != ref[4096 + i]) ok1 = 0;
        if (((uint8_t *)P2V(gap))[i] != 0) inGap = 1;
    }
    serial_puts("[prdt] sectors 0-7 in page 0: ");
    serial_puts(ok0 ? "match" : "WRONG");
    serial_puts("; sectors 8-15 in page 1: ");
    serial_puts(ok1 ? "match" : "WRONG");
    serial_puts("; frame in between written by the disk: ");
    serial_puts(inGap ? "YES" : "no");
    serial_putc('\n');
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();
    serial_init();
    boot_init();
    serial_puts("=== Lab 0x09 - AHCI & Filesystems ===\n\n");
    pmm_init();                                   /* Bài 5 */
    paging_init();                                /* Bài 6 */
    gdt_init();                                   /* Bài 7 */
    acpi_init();                                  /* Bài 8 */
    isr_init();                                   /* Bài 9 + 10 */
    timer_init();                                 /* Bài 10 */
    input_init();
    initiateKb();                                 /* Bài 11 */
    initiateMouse();
    initiateTasks();                              /* Bài 12 */
    initiateKernelThreads();
    serial_puts("[boot] pmm, paging, GDT/TSS, ACPI, IDT, APIC, timer, PS/2, tasks ready\n\n");
    initiateNetworking();                         /* Bài 13 */
    initiatePCI();                                /* Bài 14 + 15: AHCI */
    if (!ahciInfo.sata) {
        serial_puts("[kernel] no SATA disk\n");
        halt();
    }

    serial_putc('\n');
    diskDumpMbr();
    serial_putc('\n');
    fsMount("/", 0, 1);                           /* cavOS: fsMount("/", CONNECTOR_AHCI, 0, 1) */
    fsMount("/boot/", 0, 0);                      /* cavOS: fsMount("/boot/", CONNECTOR_AHCI, 0, 0) */
    serial_putc('\n');

    whichMount("/");
    whichMount("/boot/kernel.bin");
    whichMount("/bootstrap.txt");
    whichMount("/etc/motd");
    serial_putc('\n');

    fsListDir("/");
    fsListDir("/boot/");
    fsListDir("/boot/limine");
    fsListDir("/etc");
    serial_putc('\n');

    printText("/etc/motd");
    printText("/boot/limine/limine.conf");
    serial_putc('\n');

    readAndSum("/boot/kernel.bin");
    readAndSum("/big.bin");
    readAndSum("/docs/deep/nested/note.txt");
    readAndSum("/missing.txt");
    serial_putc('\n');

    prdtTest();
    serial_puts("\n[kernel] total: ");
    serial_putdec(ahciCommands);
    serial_puts(" AHCI commands, ");
    serial_putdec(ahciSectorsRead);
    serial_puts(" sectors read; cli; hlt\n");
    halt();
}
