/* timer.c — theo cavOS cpu/timer.c: initiatePitTimer + initiateApicTimer (Bài 10).
 *
 * Cách cavOS đo LAPIC timer: bật PIT ~1000 Hz qua I/O APIC, cho LAPIC đếm xuống từ
 * 0xFFFFFFFF (chia 16) trong 10 tick PIT = 10 ms, đọc còn lại bao nhiêu -> số tick LAPIC
 * mỗi ms -> nạp vào INITCNT ở chế độ periodic -> 1 ngắt / ms. Rồi che PIT.
 * Lab giữ y thứ tự đó; thêm đếm riêng PIT/LAPIC và RTC để kiểm.
 */
#include "timer.h"
#include "apic.h"
#include "isr.h"
#include "io.h"
#include "serial.h"

#ifdef NO_VOLATILE
uint64_t timerTicks;
#else
volatile uint64_t timerTicks;
#endif
volatile uint64_t pitTicks, lapicTicks;
uint32_t timerFrequency;
uint64_t apicFreq;

/* ------------------------------------------------------------------- RTC */
/* Rút gọn từ cavOS cpu/rtc.c readFromCMOS (chỉ giờ:phút:giây). */
static uint8_t cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}
static uint8_t bcd(uint8_t v, int is_bcd) {
    return is_bcd ? (uint8_t)((v & 0x0F) + (v >> 4) * 10) : v;
}
void rtc_read(uint8_t *h, uint8_t *m, uint8_t *s) {
    uint8_t s1, m1, h1;
    do {
        while (cmos(0x0A) & 0x80)              /* update in progress */
            ;
        s1 = cmos(0x00); m1 = cmos(0x02); h1 = cmos(0x04);
        while (cmos(0x0A) & 0x80)
            ;
    } while (s1 != cmos(0x00) || m1 != cmos(0x02) || h1 != cmos(0x04));
    int is_bcd = !(cmos(0x0B) & 0x04);         /* reg B bit 2 = 0 -> BCD */
    *s = bcd(s1, is_bcd);
    *m = bcd(m1, is_bcd);
    *h = bcd(h1 & 0x7F, is_bcd);
}

/* ------------------------------------------------------------------- PIT */
/* = cavOS initiatePitTimer(1000). Lệnh 0x34 = 0b00110100: kênh 0, ghi lo rồi hi,
 * mode 2 (rate generator), nhị phân. Giá trị ghi là DIVISOR: 1193182 / 1000 = 1193
 * -> PIT ra 1193182 / 1193 = 1000.15 Hz. cavOS gọi số này là "timerFrequency"
 * và in "frequency{1193MHz}". */
static void initiatePitTimer(uint32_t reload_value) {
    timerFrequency = TIMER_ACCURANCY / reload_value;
    outb(0x43, 0b00110100);
    outb(0x40, (uint8_t)(timerFrequency & 0xFF));
    outb(0x40, (uint8_t)(timerFrequency >> 8 & 0xFF));
    timerTicks = 0;
    serial_puts("[timer] PIT ch0 mode 2, divisor ");
    serial_putdec(timerFrequency);
    serial_puts(" -> 1193182/");
    serial_putdec(timerFrequency);
    serial_puts(" = ~1000.15 Hz (cavOS prints this divisor as \"frequency{1193MHz}\")\n");
}

/* cavOS: một hàm timerTick (timerTicks++; schedule(rsp)) đăng ký cho CẢ HAI vector.
 * Lab: 2 hàm để đếm riêng, cùng tăng timerTicks; chưa có scheduler (Bài 12). */
static void pitTick(AsmPassedInterrupt *r)   { (void)r; pitTicks++;   timerTicks++; }
static void lapicTick(AsmPassedInterrupt *r) { (void)r; lapicTicks++; timerTicks++; }

/* = cavOS initiateApicTimer() */
void timer_init(void) {
    uint64_t waitfor = 10;                                    /* ms */
    initiatePitTimer(1000);
    uint8_t ioapicInt = ioApicRedirect(0, 0);
    register_irq_handler(ioapicInt, pitTick);

    apicWrite(APIC_REGISTER_TIMER_DIV, 0x3);                  /* chia 16 */
    apicWrite(APIC_REGISTER_TIMER_INITCNT, 0xFFFFFFFF);

    serial_puts("[timer] calibrating: LAPIC counts down from 0xffffffff while we wait 10 PIT ticks...\n");
    uint64_t target = timerTicks + waitfor;                   /* chờ bằng PIT cũ */
    while (target > timerTicks)
        ;

    apicWrite(APIC_REGISTER_LVT_TIMER, 0x10000);              /* che LVT timer */
    uint32_t left = apicRead(APIC_REGISTER_TIMER_CURRCNT);
    uint32_t ticksInXms = 0xFFFFFFFF - left;

    uint32_t lapicId = 0;
    uint8_t  targIrq = irqPerCoreAllocate(0, &lapicId);      /* như cavOS: "GSI 0" làm khoá */
    apicFreq = ticksInXms / waitfor;

    serial_puts("[timer] LAPIC current count = ");
    serial_puthex_short(left);
    serial_puts(" -> ");
    serial_putdec(ticksInXms);
    serial_puts(" LAPIC ticks in 10 ms -> apicFreq = ");
    serial_putdec(apicFreq);
    serial_puts(" ticks/ms (div 16 -> timer input clock ~");
    serial_putdec(apicFreq * 16 / 1000);
    serial_puts(" MHz)\n");

    apicWrite(APIC_REGISTER_LVT_TIMER, targIrq | APIC_LVT_TIMER_MODE_PERIODIC);
    apicWrite(APIC_REGISTER_TIMER_DIV, 0x3);
    apicWrite(APIC_REGISTER_TIMER_INITCNT, (uint32_t)apicFreq);
    ioApicRedirect(0, 1);                                     /* che PIT cũ */
    register_irq_handler(targIrq, lapicTick);

    serial_puts("[timer] LVT timer = ");
    serial_puthex_short(apicRead(APIC_REGISTER_LVT_TIMER));
    serial_puts(" (vector ");
    serial_puthex_short(targIrq);
    serial_puts(", bit 17 periodic), INITCNT = ");
    serial_putdec(apicFreq);
    serial_puts(" -> one interrupt per ms\n");
}
