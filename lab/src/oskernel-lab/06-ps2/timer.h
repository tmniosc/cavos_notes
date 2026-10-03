/* timer.h — PIT + LAPIC timer + RTC, theo cavOS cpu/timer.c + cpu/rtc.c (Bài 10). */
#pragma once
#include <stdint.h>

#define TIMER_ACCURANCY 1193182      /* tần số PIT 8254 (Hz), tên y như cavOS */

/* cavOS include/timer.h khai "uint64_t timerTicks;" KHÔNG volatile. cavOS biên dịch
 * không -O nên vòng chờ vẫn đọc lại biến; lab dùng -O2 nên phải volatile.
 * Build DEMO=-DNO_VOLATILE để thấy vòng chờ treo. */
#ifdef NO_VOLATILE
extern uint64_t timerTicks;
#else
extern volatile uint64_t timerTicks;
#endif
extern volatile uint64_t pitTicks, lapicTicks;   /* lab: đếm riêng từng nguồn */
extern uint32_t timerFrequency;                   /* cavOS đặt tên vậy, thật ra là divisor PIT */
extern uint64_t apicFreq;                         /* cavOS đặt tên vậy, thật ra là tick LAPIC / 1 ms */

void timer_init(void);               /* = cavOS initiateApicTimer() */

/* = cavOS sleep(time): chờ 'time' tick (1 tick ~ 1 ms). cavOS gọi handControl() trong vòng
 * chờ (nhường CPU cho task khác); lab chưa có task nên hlt tới ngắt kế tiếp.
 * Lab 0x06: initiateMouse() cần hàm này ("mouse needs a timer"). */
void sleep(uint32_t time);

/* RTC CMOS: đồng hồ độc lập để kiểm số tick mỗi giây. */
void rtc_read(uint8_t *h, uint8_t *m, uint8_t *s);
