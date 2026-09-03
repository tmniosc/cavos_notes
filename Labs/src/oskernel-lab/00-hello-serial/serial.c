/* serial.c — UART 16550, COM1 @ 0x3F8, 115200 8N1 (xem [[UART 16550]]).
 * Divisor = 115200 / 115200 = 1 -> DLL=1, DLM=0.
 */
#include "serial.h"
#include "io.h"

#define COM1 0x3F8

#define REG_DATA        0   /* DLAB=0: RBR/THR ; DLAB=1: divisor low  */
#define REG_INT_ENABLE  1   /* DLAB=0: IER     ; DLAB=1: divisor high */
#define REG_FIFO_CTRL   2
#define REG_LINE_CTRL   3
#define REG_MODEM_CTRL  4
#define REG_LINE_STATUS 5

#define LSR_THRE (1 << 5)   /* Transmitter Holding Register Empty */

void serial_init(void) {
    outb(COM1 + REG_INT_ENABLE, 0x00);  /* tắt ngắt */
    outb(COM1 + REG_LINE_CTRL,  0x80);  /* bật DLAB để ghi divisor */
    outb(COM1 + REG_DATA,       0x01);  /* divisor low  = 1 -> 115200 baud */
    outb(COM1 + REG_INT_ENABLE, 0x00);  /* divisor high = 0 */
    outb(COM1 + REG_LINE_CTRL,  0x03);  /* DLAB=0, 8 bit, no parity, 1 stop */
    outb(COM1 + REG_FIFO_CTRL,  0xC7);  /* bật FIFO, xoá, ngưỡng 14 byte */
    outb(COM1 + REG_MODEM_CTRL, 0x0B);  /* DTR + RTS + OUT2 */
}

void serial_putc(char c) {
    if (c == '\n')
        serial_putc('\r');
    while ((inb(COM1 + REG_LINE_STATUS) & LSR_THRE) == 0)
        ;                                /* chờ THR rỗng mới ghi */
    outb(COM1 + REG_DATA, (uint8_t)c);
}

void serial_puts(const char *s) {
    for (; *s; s++)
        serial_putc(*s);
}
