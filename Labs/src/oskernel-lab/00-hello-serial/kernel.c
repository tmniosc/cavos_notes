/* Lab 0x00 — Hello Serial (Step 00 + Step 01).
 * Limine nạp ELF64 vào long mode rồi nhảy thẳng vào kmain (ENTRY trong linker.ld).
 */
#include "../limine.h"
#include "serial.h"

/* Base revision 2 — khớp cavOS gốc (xem [[Limine Protocol]] §4). */
LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    serial_puts("=== Lab 0x00 - Hello Serial (Step 00 + 01) ===\n");
    serial_puts("Hello Serial\n");
    serial_puts("[done] halt.\n");

    halt();
}
