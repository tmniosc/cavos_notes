/* Lab 0x04 — GDT, TSS & IDT (Bài 7 + Bài 9).
 * Thứ tự giống cavOS _start(): ... initiatePMM -> initiateVMM -> initiateGDT -> initiateISR.
 * Demo: in trạng thái GDT/TSS/IDT, rồi gây exception có chủ đích:
 *   int3 (trap, quay lại), #DE, #UD, int trên cổng trống (được báo rồi chạy tiếp),
 *   cuối cùng #PF ở địa chỉ chưa map -> in CR2 -> panic, halt.
 * Build khác: DEMO=-DDF_DEMO (stack hỏng -> #DF trên IST1), thêm -DNO_IST (như cavOS -> triple fault).
 */
#include "../limine.h"
#include "boot.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "serial.h"

LIMINE_BASE_REVISION(2)

#define UNMAPPED_VA 0x0000500000000000ULL     /* nửa thấp, không ai map */

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static void check(const char *what) {
    serial_puts(isr_expect_hit() ? "[demo] " : "[demo] NOT HIT: ");
    serial_puts(what);
    serial_puts(isr_expect_hit() ? " -> back in kmain\n\n" : "\n\n");
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    boot_init();

    serial_puts("=== Lab 0x04 - GDT, TSS & IDT ===\n\n");

    /* Bài 5 + 6: giữ nguyên từ Lab 0x03, chỉ in 1 dòng. */
    pmm_init();
    paging_init();
    serial_puts("[mem] pmm + own PML4 ready, free frames = ");
    serial_putdec(pmm_free_count());
    serial_puts("\n\n");

    /* ---------------- Bài 7: GDT + TSS ---------------- */
    serial_puts("[gdt] before: CS=");
    {
        uint16_t cs;
        __asm__ volatile("mov %%cs, %0" : "=r"(cs));
        serial_puthex_short(cs);
    }
    serial_puts(" (Limine's GDT)\n");
    gdt_init();
    serial_puts("[gdt] lgdt + lretq + ltr done\n");

    /* ---------------- Bài 9: PIC + IDT ---------------- */
    isr_init();
    pic_dump();
    gdt_dump();
    idt_dump();
    serial_putc('\n');

#ifdef DF_DEMO
    /* Stack hỏng: đẩy khung ngắt vào trang chưa map -> #PF khi đang vào #PF -> #DF. */
    serial_puts("[demo] RSP = unmapped page, then int3\n");
    __asm__ volatile("mov %0, %%rsp\n\t"
                     "int3\n\t"
                     : : "r"(UNMAPPED_VA + 0x1000) : "memory");
    halt();
#endif

    /* 1. int3 = #BP, trap: RIP trong khung đã trỏ sau int3. */
    {
        uint64_t after;
        isr_expect(0, 0);
        __asm__ volatile("int3\n\t"
                         "1: lea 1b(%%rip), %0" : "=r"(after) : : "memory");
        serial_puts("[demo] address right after int3 = ");
        serial_puthex(after);
        serial_putc('\n');
        check("int3 returned");
    }

    /* 2. chia cho 0: "div %ecx" (F7 F1, 2 byte) với ecx = 0 -> #DE, fault. */
    {
        uint64_t at;
        isr_expect(1ULL << 0, 2);
        __asm__ volatile("lea 1f(%%rip), %0\n\t"
                         "xor %%edx, %%edx\n\t"
                         "mov $1, %%eax\n\t"
                         "xor %%ecx, %%ecx\n\t"
                         "1: div %%ecx\n\t"
                         : "=&r"(at) : : "rax", "rcx", "rdx", "memory");
        serial_puts("[demo] address of the div = ");
        serial_puthex(at);
        serial_putc('\n');
        check("#DE reported");
    }

    /* 3. ud2 (0F 0B, 2 byte) -> #UD. */
    {
        uint64_t at;
        isr_expect(1ULL << 6, 2);
        __asm__ volatile("lea 1f(%%rip), %0\n\t"
                         "1: ud2\n\t" : "=r"(at) : : "memory");
        serial_puts("[demo] address of the ud2 = ");
        serial_puthex(at);
        serial_putc('\n');
        check("#UD reported");
    }

    /* 4. int 0x40 (CD 40, 2 byte): cổng 0x40 để trống (P=0) như cavOS. */
    {
        isr_expect((1ULL << 11) | (1ULL << 13), 2);   /* #NP hoặc #GP */
        __asm__ volatile("int $0x40" : : : "memory");
        check("int 0x40 on an empty gate reported");
    }

    /* 5. int 0x80: cổng syscall có thật (DPL 3). */
    __asm__ volatile("mov $0x1234, %%eax\n\tint $0x80" : : : "rax", "memory");
    serial_puts("[demo] int 0x80 returned\n\n");

    /* 6. #PF thật: đọc VA chưa map -> không "expect" -> panic + halt. */
    serial_puts("[demo] reading unmapped VA ");
    serial_puthex(UNMAPPED_VA);
    serial_puts(" ...\n");
    volatile uint64_t *p = (volatile uint64_t *)UNMAPPED_VA;
    uint64_t v = *p;
    serial_puts("[demo] NOT REACHED, read ");
    serial_puthex(v);
    serial_putc('\n');
    halt();
}
