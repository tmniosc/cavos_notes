/* kout.h — in một DÒNG ra serial mà không bị task khác chen giữa (Lab 0x07).
 * Mỗi dòng gồm nhiều lần serial_puts; timer có thể đổi task ở giữa bất kỳ lần nào.
 * kout_begin() lấy spinlock LOCK_OUT (= LOCK_DEBUGF trong debugf() của cavOS drivers/serial.c),
 * in tiền tố "[t=ticks] [id name] ", kout_end() nhả khoá.
 * Build DEMO=-DNO_PRINT_LOCK để bỏ khoá và xem các dòng trộn vào nhau. */
#pragma once
#include <stdint.h>

void kout_begin(void);             /* khoá + tiền tố của task hiện tại */
void kout_begin_raw(void);         /* chỉ khoá */
void kout_end(void);
