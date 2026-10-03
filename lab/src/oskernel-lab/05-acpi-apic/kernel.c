/* Lab 0x05 — ACPI, APIC & Timer (Bài 8 + Bài 10).
 * Thứ tự giống cavOS _start():
 *   initiatePMM -> initiateVMM -> initiateGDT -> initiateACPI -> initiateISR (gọi
 *   initiateAPIC + sti) -> ... -> initiateApicTimer.
 * Demo: liệt kê bảng ACPI + giải MADT/FADT, bật LAPIC, đo LAPIC timer bằng PIT,
 * chuyển sang LAPIC periodic 1 ms, đếm tick theo từng giây RTC, rồi dừng.
 */
#include "../limine.h"
#include "boot.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "acpi.h"
#include "apic.h"
#include "timer.h"
#include "serial.h"

LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static void put2(uint8_t v) {
    serial_putc('0' + v / 10);
    serial_putc('0' + v % 10);
}

static void put_hms(uint8_t h, uint8_t m, uint8_t s) {
    put2(h); serial_putc(':'); put2(m); serial_putc(':'); put2(s);
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    boot_init();

    serial_puts("=== Lab 0x05 - ACPI, APIC & Timer ===\n\n");

    pmm_init();                                   /* Bài 5 */
    paging_init();                                /* Bài 6 */
    gdt_init();                                   /* Bài 7 */
    serial_puts("[boot] pmm + own PML4 + GDT/TSS ready\n\n");

    /* ---------------- Bài 8: ACPI ---------------- */
    acpi_init();                                  /* = initiateACPI() (phần bảng) */
    acpi_dump();
    serial_putc('\n');

    /* ---------------- Bài 9 + 10: IDT, APIC, sti ---------------- */
    isr_init();                                   /* = initiateISR(): ... apic_init(); sti */
    apic_dump();
    serial_putc('\n');

    /* ---------------- Bài 10: timer ---------------- */
    timer_init();                                 /* = initiateApicTimer() */
    serial_puts("[ioapic] redirection entries after timer_init:\n");
    ioapic_dump_entries(0, 4);
    serial_putc('\n');

    /* Đếm tick giữa các lần giây RTC đổi: số tick thật mỗi giây. */
    uint8_t h, m, s, s0;
    rtc_read(&h, &m, &s0);
    do {                                          /* chờ tới mép giây đầu tiên */
        __asm__ volatile("hlt");
        rtc_read(&h, &m, &s);
    } while (s == s0);
    uint64_t l_prev = lapicTicks, p_prev = pitTicks;
    serial_puts("[uptime] RTC ");
    put_hms(h, m, s);
    serial_puts("  start: timerTicks=");
    serial_putdec(timerTicks);
    serial_puts(" (lapic ");
    serial_putdec(l_prev);
    serial_puts(", pit ");
    serial_putdec(p_prev);
    serial_puts(")\n");

    for (int i = 0; i < 5; i++) {
        s0 = s;
        do {
            __asm__ volatile("hlt");              /* ngủ tới ngắt kế tiếp */
            rtc_read(&h, &m, &s);
        } while (s == s0);
        uint64_t t = timerTicks, l = lapicTicks, p = pitTicks;
        serial_puts("[uptime] RTC ");
        put_hms(h, m, s);
        serial_puts("  timerTicks=");
        serial_putdec(t);
        serial_puts("  uptime ~");
        serial_putdec(t / 1000);
        serial_putc('.');
        serial_putc('0' + (t / 100) % 10);
        serial_puts(" s  this second: +");
        serial_putdec(l - l_prev);
        serial_puts(" LAPIC, +");
        serial_putdec(p - p_prev);
        serial_puts(" PIT\n");
        l_prev = l; p_prev = p;
    }

    serial_puts("[lapic] spurious interrupts seen: ");
    serial_putdec(spurious_count);
    serial_puts("\n[kernel] done, cli; hlt\n");
    halt();
}
