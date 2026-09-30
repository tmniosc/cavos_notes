/* serial.h — UART 16550 trên COM1 (Step 01) + 2 helper in số (Lab 0x01). */
#pragma once
#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(uint64_t v);   /* in 0x + 16 chữ số hex (zero-pad) */
void serial_putdec(uint64_t v);
