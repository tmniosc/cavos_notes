/* sse.c — = cavOS cpu/system.c initiateSSE() (Bài 17).
 *
 * Giống cavOS: CPUID.1 EDX bit 25 (SSE) và bit 24 (FXSR) bắt buộc; CR0: xoá EM (bit 2), đặt MP
 * (bit 1); CR4: OSFXSR (bit 9) + OSXMMEXCPT (bit 10); fninit; CR0.NE (bit 5); nếu CPUID.1 ECX
 * bit 26 (XSAVE): CR4.OSXSAVE (bit 18), và nếu thêm bit 28 (AVX): XCR0 = 7 (x87 | SSE | AVX).
 * Thêm của lab: in CR0/CR4/XCR0 trước và sau, và cỡ vùng XSAVE mà CPUID.0xD báo.
 */
#include "sse.h"
#include "serial.h"

int sseAvxEnabled;
volatile int sseReady;

static void cpuid(uint32_t leaf, uint32_t sub, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(sub));
}
static uint64_t rdcr0(void) { uint64_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static uint64_t rdcr4(void) { uint64_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v; }

void sseDump(const char *when) {
    uint64_t cr0 = rdcr0(), cr4 = rdcr4();
    serial_puts("[sse] ");
    serial_puts(when);
    serial_puts(": CR0 ");
    serial_puthex_short(cr0);
    serial_puts(" (EM=");
    serial_putdec((cr0 >> 2) & 1);
    serial_puts(" MP=");
    serial_putdec((cr0 >> 1) & 1);
    serial_puts(" TS=");
    serial_putdec((cr0 >> 3) & 1);
    serial_puts(" NE=");
    serial_putdec((cr0 >> 5) & 1);
    serial_puts("), CR4 ");
    serial_puthex_short(cr4);
    serial_puts(" (OSFXSR=");
    serial_putdec((cr4 >> 9) & 1);
    serial_puts(" OSXMMEXCPT=");
    serial_putdec((cr4 >> 10) & 1);
    serial_puts(" OSXSAVE=");
    serial_putdec((cr4 >> 18) & 1);
    serial_putc(')');
    if ((cr4 >> 18) & 1) {
        uint32_t lo, hi;
        __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        serial_puts(", XCR0 ");
        serial_puthex_short(((uint64_t)hi << 32) | lo);
    }
    serial_putc('\n');
}

void initiateSSE(void) {
    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);
    serial_puts("[sse] CPUID.1: EDX ");
    serial_puthex_short(d);
    serial_puts(" (SSE bit25=");
    serial_putdec((d >> 25) & 1);
    serial_puts(" FXSR bit24=");
    serial_putdec((d >> 24) & 1);
    serial_puts("), ECX ");
    serial_puthex_short(c);
    serial_puts(" (XSAVE bit26=");
    serial_putdec((c >> 26) & 1);
    serial_puts(" AVX bit28=");
    serial_putdec((c >> 28) & 1);
    serial_puts(")\n");
    if (!((d >> 25) & 1) || !((d >> 24) & 1)) {
        serial_puts("[sse] FATAL! No support for SSE/FXSR found!\n");
        for (;;) __asm__ volatile("cli; hlt");
    }

    /* enable SSE: CR0.EM = 0, CR0.MP = 1, CR4.OSFXSR = CR4.OSXMMEXCPT = 1 */
    __asm__ volatile("mov %%cr0, %%rax; and $0xFFFB, %%ax; or $2, %%eax; mov %%rax, %%cr0;"
                     "mov %%cr4, %%rax; or $0b11000000000, %%rax; mov %%rax, %%cr4;"
                     : : : "rax");
    /* set NE in cr0 and reset x87 fpu */
    __asm__ volatile("fninit; mov %%cr0, %%rax; or $0b100000, %%rax; mov %%rax, %%cr0;" : : : "rax");

    if (c & (1u << 26)) {
        serial_puts("[cpu] The xsave instruction is available. Enabling..\n");
        uint64_t cr4 = rdcr4() | (1u << 18);
        __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
        if (c & (1u << 28)) {
            serial_puts("[cpu] The AVX extensions are available. Enabling..\n");
            __asm__ volatile("xsetbv" : : "c"(0), "a"(7), "d"(0));   /* x87 | SSE | AVX */
            sseAvxEnabled = 1;
        }
        uint32_t ea, eb, ec, ed;
        cpuid(0xD, 0, &ea, &eb, &ec, &ed);
        serial_puts("[sse] CPUID.0xD: XSAVE area for the bits now in XCR0 = ");
        serial_putdec(eb);
        serial_puts(" bytes (fxsave: 512)\n");
    }
    sseReady = 1;
}
