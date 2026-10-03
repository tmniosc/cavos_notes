/* idt.c — theo cavOS cpu/idt.c: mảng static 256 cổng + set_idt_gate + lidt. */
#include "idt.h"
#include "gdt.h"
#include "serial.h"

static idt_gate_t     idt[IDT_ENTRIES];        /* .bss -> mọi cổng = 0 (P=0) */
static idt_register_t idt_reg;

void set_idt_gate(int n, uint64_t handler, uint8_t flags, uint8_t ist) {
    idt[n].isr_low    = (uint16_t)handler;
    idt[n].kernel_cs  = GDT_KERNEL_CODE;
    idt[n].ist        = ist;                   /* cavOS: luôn 0 */
    idt[n].reserved   = 0;
    idt[n].attributes = flags;
    idt[n].isr_mid    = (uint16_t)(handler >> 16);
    idt[n].isr_high   = (uint32_t)(handler >> 32);
}

void set_idt(void) {
    idt_reg.base  = (uint64_t)&idt;
    idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;   /* 4095 */
    __asm__ volatile("lidt %0" : : "m"(idt_reg) : "memory");
}

static void dump_gate(int n) {
    const uint64_t *q = (const uint64_t *)&idt[n];
    uint64_t off = (uint64_t)idt[n].isr_low | ((uint64_t)idt[n].isr_mid << 16) |
                   ((uint64_t)idt[n].isr_high << 32);
    uint8_t a = idt[n].attributes;

    serial_puts("[idt]   vec ");
    serial_puthex_short((uint64_t)n);
    serial_puts(n < 0x10 ? "  " : " ");
    serial_puthex(q[1]);
    serial_putc(' ');
    serial_puthex(q[0]);
    serial_puts("  off=");
    serial_puthex(off);
    serial_puts(" sel=");
    serial_puthex_short(idt[n].kernel_cs);
    serial_puts(" ist=");
    serial_putdec(idt[n].ist);
    serial_puts(" P=");
    serial_putdec(a >> 7);
    serial_puts(" DPL=");
    serial_putdec((a >> 5) & 3);
    serial_puts((a & 0xf) == 0xe ? " interrupt-gate\n"
                : (a & 0xf) == 0xf ? " trap-gate\n" : " (empty)\n");
}

void idt_dump(void) {
    idt_register_t cur;
    __asm__ volatile("sidt %0" : "=m"(cur));

    serial_puts("[idt] sidt: base = ");
    serial_puthex(cur.base);
    serial_puts("  limit = ");
    serial_puthex_short(cur.limit);
    serial_puts(" (");
    serial_putdec((cur.limit + 1) / sizeof(idt_gate_t));
    serial_puts(" gates x 16 bytes)\n");

    int present = 0;
    for (int i = 0; i < IDT_ENTRIES; i++)
        if (idt[i].attributes & 0x80)
            present++;
    serial_puts("[idt] present gates = ");
    serial_putdec(present);
    serial_puts(" (0-47, 0x80, 0xff like cavOS; the rest P=0)\n");

    static const int show[] = {0x00, 0x03, 0x08, 0x0e, 0x20, 0x40, 0x80, 0xff};
    for (unsigned i = 0; i < sizeof(show) / sizeof(show[0]); i++)
        dump_gate(show[i]);
}
