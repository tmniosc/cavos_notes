/* kernel_helper.c — = cavOS entry/kernel_helper.c (Bài 12).
 *
 * Vì sao cần một thread riêng để dọn task chết: task đang chết vẫn đứng trên stack của
 * chính nó (và PML4 của nó đang nằm trong CR3), nên không tự trả stack/PML4 được. Nó
 * giao mình cho reaper (taskCallReaper), đặt DEAD rồi chờ tick. Thread helper chạy ở ngữ
 * cảnh khác, thấy state == DEAD thì trả tài nguyên và gỡ task khỏi danh sách.
 *
 * Khác cavOS: kernelHelpEntry của cavOS lặp handControl() -> helper LUÔN READY, nên CPU
 * không bao giờ rơi vào dummyTask. Lab mặc định ngủ 5 ms giữa 2 vòng (taskSleepMs) để thấy
 * task idle chạy; build DEMO=-DHELPER_SPIN để làm y cavOS.
 */
#include "kernel_helper.h"
#include "task.h"
#include "kout.h"
#include "pmm.h"
#include "serial.h"
#include "timer.h"

ReapedInfo reaped[16];
int        reapedCount;

static void helperReaper(void) {
    Task *t = reaperTask;
    if (!t || t->state != TASK_STATE_DEAD)
        return;                              /* cavOS: if (state != DEAD) goto end */

    ReapedInfo *r = &reaped[reapedCount < 16 ? reapedCount++ : 15];
    r->id = t->id;
    r->name = t->cmdline;
    r->cpuTicks = t->cpuTicks;
    r->switchesIn = t->switchesIn;
    r->wakeups = t->wakeups;
    r->reapedAt = timerTicks;
    r->frames = r->tables = 0;

    uint64_t before = pmm_free_count();
    taskFreeResources(t, &r->frames, &r->tables);   /* free stacks + pagedir */
    taskListDestroy(t);                              /* free the task now that it's safe */
    uint64_t after = pmm_free_count();

    kout_begin();
    serial_puts("reaper: task ");
    serial_putdec(r->id);
    serial_puts(" (");
    serial_puts(r->name);
    serial_puts(") freed: ");
    serial_putdec(r->frames);
    serial_puts(" frames (4 stack + 1 TSS stack) + ");
    serial_putdec(r->tables);
    serial_puts(" page tables (PDPT, PD, PT, PML4); pmm free ");
    serial_putdec(before);
    serial_puts(" -> ");
    serial_putdec(after);
    serial_putc('\n');
    kout_end();

    reaperTask = 0;                          /* done! continue listening */
}

static void kernelHelpEntry(void) {
    while (1) {
        helperReaper();
#ifdef HELPER_SPIN
        handControl();                       /* y cavOS */
#else
        taskSleepMs(5);
#endif
    }
}

void initiateKernelThreads(void) {
    Task *t = taskCreateKernel((uint64_t)kernelHelpEntry, 0);
    taskNameKernel(t, "helper");             /* cavOS: helperCmdline = "kernel" */
}
