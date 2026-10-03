/* gdt.c — chép theo cavOS cpu/gdt.c: initiateGDT -> gdt_reload -> gdt_load_tss.
 * Giá trị access/granularity y hệt cavOS. Khác duy nhất: thêm gdt_dump() để in trạng thái.
 */
#include "gdt.h"
#include "boot.h"
#include "serial.h"

static GDTEntries gdt;
static GDTPtr     gdtr;
static TSSPtr     tss;

TSSPtr *tssPtr = &tss;

static void set_desc(int i, uint16_t limit, uint8_t access, uint8_t gran) {
    gdt.descriptors[i].limit       = limit;
    gdt.descriptors[i].base_low    = 0;
    gdt.descriptors[i].base_mid    = 0;
    gdt.descriptors[i].access      = access;
    gdt.descriptors[i].granularity = gran;
    gdt.descriptors[i].base_high   = 0;
}

/* = cavOS gdt_load_tss(): điền base của TSS vào descriptor 0x58 rồi ltr. */
static void gdt_load_tss(TSSPtr *t) {
    uint64_t addr = (uint64_t)t;

    gdt.tss.base_low     = (uint16_t)addr;
    gdt.tss.base_mid     = (uint8_t)(addr >> 16);
    gdt.tss.flags1       = 0x89;          /* 0b10001001: P=1, DPL=0, type=1001 */
    gdt.tss.flags2       = 0;
    gdt.tss.base_high    = (uint8_t)(addr >> 24);
    gdt.tss.base_upper32 = (uint32_t)(addr >> 32);
    gdt.tss.reserved     = 0;

    __asm__ volatile("ltr %0" : : "rm"((uint16_t)GDT_TSS_SEL) : "memory");
}

/* = cavOS gdt_reload(): lgdt, đổi CS bằng far return, nạp lại DS..SS. */
static void gdt_reload(void) {
    __asm__ volatile("lgdt %0\n\t"
                     "push $0x28\n\t"            /* CS mới */
                     "lea 1f(%%rip), %%rax\n\t"
                     "push %%rax\n\t"            /* RIP mới = nhãn 1 */
                     "lretq\n\t"                 /* pop RIP, pop CS */
                     "1:\n\t"
                     "mov $0x30, %%eax\n\t"
                     "mov %%eax, %%ds\n\t"
                     "mov %%eax, %%es\n\t"
                     "mov %%eax, %%fs\n\t"
                     "mov %%eax, %%gs\n\t"
                     "mov %%eax, %%ss\n\t"
                     :
                     : "m"(gdtr)
                     : "rax", "memory");
}

void gdt_init(void) {
    set_desc(0, 0, 0, 0);                       /* 0x00 null                    */
    set_desc(1, 0xffff, 0x9a, 0x00);            /* 0x08 kernel code 16 (di sản) */
    set_desc(2, 0xffff, 0x92, 0x00);            /* 0x10 kernel data 16          */
    set_desc(3, 0xffff, 0x9a, 0xcf);            /* 0x18 kernel code 32          */
    set_desc(4, 0xffff, 0x92, 0xcf);            /* 0x20 kernel data 32          */
    set_desc(5, 0, 0x9a, 0x20);                 /* 0x28 kernel code 64 (L=1)    */
    set_desc(6, 0, 0x92, 0x00);                 /* 0x30 kernel data 64          */
    set_desc(7, 0, 0, 0);                       /* 0x38 trống ("SYSENTER")      */
    set_desc(8, 0, 0, 0);                       /* 0x40 trống                   */
    set_desc(9, 0, 0xf2, 0x00);                 /* 0x48 user data 64 (DPL 3)    */
    set_desc(10, 0, 0xfa, 0x20);                /* 0x50 user code 64 (DPL 3, L) */

    gdt.tss.length       = 104;                 /* như cavOS (limit chuẩn là 103) */
    gdt.tss.base_low     = 0;
    gdt.tss.base_mid     = 0;
    gdt.tss.flags1       = 0x89;
    gdt.tss.flags2       = 0;
    gdt.tss.base_high    = 0;
    gdt.tss.base_upper32 = 0;
    gdt.tss.reserved     = 0;

    gdtr.limit = sizeof(GDTEntries) - 1;        /* 0x67 */
    gdtr.base  = (uint64_t)&gdt;

    gdt_reload();

    memset8(&tss, 0, sizeof(TSSPtr));           /* rsp0 = 0, iopb = 0 như cavOS */
    gdt_load_tss(&tss);
}

void tss_set_ist1(uint64_t stack_top) {
    tss.ist1 = stack_top;
}

/* ---------------- in trạng thái để trang lab giải mã ---------------- */

static uint16_t read_sel(int which) {
    uint16_t v = 0;
    switch (which) {
    case 0: __asm__ volatile("mov %%cs, %0" : "=r"(v)); break;
    case 1: __asm__ volatile("mov %%ds, %0" : "=r"(v)); break;
    case 2: __asm__ volatile("mov %%es, %0" : "=r"(v)); break;
    case 3: __asm__ volatile("mov %%fs, %0" : "=r"(v)); break;
    case 4: __asm__ volatile("mov %%gs, %0" : "=r"(v)); break;
    case 5: __asm__ volatile("mov %%ss, %0" : "=r"(v)); break;
    default: __asm__ volatile("str %0" : "=r"(v)); break;
    }
    return v;
}

static const char *gdt_names[13] = {
    "null", "kernel code 16", "kernel data 16", "kernel code 32",
    "kernel data 32", "kernel code 64", "kernel data 64", "(empty, SYSENTER)",
    "(empty)", "user data 64", "user code 64", "TSS low", "TSS high"};

void gdt_dump(void) {
    GDTPtr cur;
    __asm__ volatile("sgdt %0" : "=m"(cur));

    serial_puts("[gdt] sgdt: base = ");
    serial_puthex(cur.base);
    serial_puts("  limit = ");
    serial_puthex_short(cur.limit);
    serial_puts(" (");
    serial_putdec(cur.limit + 1);
    serial_puts(" bytes)\n");

    static const char *sn[7] = {"CS", "DS", "ES", "FS", "GS", "SS", "TR"};
    serial_puts("[gdt] selectors:");
    for (int i = 0; i < 7; i++) {
        uint16_t s = read_sel(i);
        serial_puts(" ");
        serial_puts(sn[i]);
        serial_puts("=");
        serial_puthex_short(s);
    }
    serial_putc('\n');

    const uint8_t *b = (const uint8_t *)&gdt;
    for (int i = 0; i < 13; i++) {
        uint64_t q;
        __builtin_memcpy(&q, b + i * 8, 8);           /* struct packed: đọc từng qword */
        serial_puts("[gdt]   +");
        serial_puthex_short((uint64_t)i * 8);
        serial_puts(i * 8 < 16 ? "  " : " ");
        serial_puthex(q);
        serial_puts("  ");
        serial_puts(gdt_names[i]);
        serial_putc('\n');
    }

    serial_puts("[tss] base = ");
    serial_puthex((uint64_t)&tss);
    serial_puts("  size = ");
    serial_putdec(sizeof(TSSPtr));
    serial_puts("  rsp0 = ");
    serial_puthex_short(tss.rsp0);
    serial_puts("  ist1 = ");
    serial_puthex(tss.ist1);
    serial_puts("  iomap base = ");
    serial_puthex_short(tss.iopb >> 16);
    serial_putc('\n');
}
