/* serial.h — UART 16550 trên COM1 (Step 01). */
#pragma once
#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
