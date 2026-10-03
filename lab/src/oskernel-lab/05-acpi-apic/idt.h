/* idt.h — IDT 256 cổng, giống cavOS src/kernel/include/idt.h + cpu/idt.c (Bài 9). */
#pragma once
#include <stdint.h>

typedef struct {                   /* 1 cổng ngắt 64-bit = 16 byte */
    uint16_t isr_low;              /* offset handler 15:0  */
    uint16_t kernel_cs;            /* selector CS khi vào handler (0x28) */
    uint8_t  ist;                  /* 0 = không đổi stack; 1..7 = TSS.istN */
    uint8_t  attributes;           /* P | DPL | 0 | type (0xE interrupt, 0xF trap) */
    uint16_t isr_mid;              /* offset 31:16 */
    uint32_t isr_high;             /* offset 63:32 */
    uint32_t reserved;
} __attribute__((packed)) idt_gate_t;

typedef struct {                   /* giá trị nạp vào IDTR */
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256

/* cavOS: set_idt_gate(n, handler, flags) luôn ghi ist = 0.
 * Lab thêm tham số ist để thử IST1 cho #DF (cavOS KHÔNG làm). */
void set_idt_gate(int n, uint64_t handler, uint8_t flags, uint8_t ist);
void set_idt(void);                /* lidt */
void idt_dump(void);               /* in IDTR + vài cổng đã giải mã */
