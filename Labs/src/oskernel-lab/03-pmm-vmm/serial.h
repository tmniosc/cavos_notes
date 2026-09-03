/* serial.h — UART 16550 COM1 (Step 01) + helper in số/căn cột cho bảng dump. */
#pragma once
#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(uint64_t v);                        /* 0x + 16 hex, zero-pad */
void serial_putdec(uint64_t v);

void serial_spaces(uint64_t n);
void serial_repeat(char c, uint64_t n);
void serial_puts_pad(const char *s, uint64_t width);   /* căn trái, đệm space  */
void serial_putdec_right(uint64_t v, uint64_t width);  /* căn phải             */
