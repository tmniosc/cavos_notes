/* kout.c — khoá dòng in ra serial (Lab 0x07). */
#include "kout.h"
#include "serial.h"
#include "spinlock.h"
#include "task.h"
#include "timer.h"

static Spinlock LOCK_OUT;

void kout_begin_raw(void) {
#ifndef NO_PRINT_LOCK
    spinlockAcquire(&LOCK_OUT);
#endif
}

void kout_begin(void) {
    kout_begin_raw();
    serial_puts("[t=");
    serial_putdec_right(timerTicks, 5);
    serial_puts("] [");
    serial_putdec(currentTask->id);
    serial_putc(' ');
    serial_puts_pad(currentTask->cmdline, 7);
    serial_puts("] ");
}

void kout_end(void) {
#ifndef NO_PRINT_LOCK
    spinlockRelease(&LOCK_OUT);
#endif
}
