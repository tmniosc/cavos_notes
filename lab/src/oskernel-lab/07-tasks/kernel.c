/* Lab 0x07 — Multitasking & Scheduler (Bài 12).
 * Thứ tự giống cavOS _start():
 *   ... initiateApicTimer -> fsMount("/dev/") -> initiateKb -> initiateMouse
 *   -> initiateTasks()          ("any filesystem operations depend on currentTask")
 *   -> initiateKernelThreads()  (thread helper: reaper)
 * Từ initiateTasks() trở đi kmain chính là "task 0"; mỗi tick LAPIC (1 ms) timerTick ->
 * schedule() đổi sang task READY kế tiếp.
 * Demo (in bằng kout_begin/kout_end để mỗi dòng không bị chen):
 *   worker A, B, C  — vòng lặp bận, KHÔNG nhường CPU: chỉ timer mới cắt được (preemption).
 *                     C return sau 2 vòng -> taskKernelReturn -> taskKill -> reaper dọn.
 *   sleeper         — taskSleepMs(50): BLOCKED, scheduler đánh thức khi tới giờ.
 *   poller          — sleep(50) kiểu cavOS: vẫn READY, mỗi lượt chỉ nhường CPU.
 *   kbd             — chờ phím ở WAITING_INPUT, IRQ1 đánh thức; gõ "quit" + Enter thì thoát.
 *   task 0 (kmain)  — chờ mọi thread xong rồi in bảng tick CPU từng task.
 * Phím thật đến từ QEMU monitor: scripts/tasks_input.py.
 * DEMO: -DNO_PRINT_LOCK (dòng in bị trộn), -DLOST_WAKEUP (mất lần đánh thức),
 *       -DHELPER_SPIN (helper quay như cavOS -> dummy không bao giờ chạy).
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
#include "io.h"
#include "ps2.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "serial.h"
#include "task.h"
#include "kernel_helper.h"
#include "kout.h"

LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static volatile int workersDone, sleeperDone, pollerDone, kbdDone;

/* ---------------------------------------------------------------- tiện ích */
static uint64_t read_rsp(void) {
    uint64_t v;
    __asm__ volatile("mov %%rsp, %0" : "=r"(v));
    return v;
}
static uint64_t read_rflags(void) {
    uint64_t v;
    __asm__ volatile("pushfq; pop %0" : "=r"(v));
    return v;
}

/* Dòng đầu tiên mỗi thread in: chứng minh mỗi task có stack + CR3 riêng. */
static void hello_line(uint64_t rdi) {
    volatile uint64_t local = 0;
    uint64_t va = (uint64_t)&local;
    kout_begin();
    serial_puts("started: rdi=");
    serial_putdec(rdi);
    serial_puts(" RSP=");
    serial_puthex_short(read_rsp());
    serial_puts(" &local=");
    serial_puthex_short(va);
    serial_puts(" -> PA ");
    serial_puthex_short(vresolve_in(currentTask->pagedir, va));
    serial_puts(" CR3=");
    serial_puthex_short(paging_read_cr3());
    serial_puts(" IF=");
    serial_putdec((read_rflags() >> 9) & 1);
    serial_putc('\n');
    kout_end();
}

/* ------------------------------------------------------------------ threads */
#define WORKER_ROUNDS 4
#define WORKER_SLICE  15                      /* tick CPU của RIÊNG worker mỗi vòng */

static void worker(uint64_t rdi) {
    hello_line(rdi);
    int rounds = (rdi == 2) ? 2 : WORKER_ROUNDS;  /* worker C (rdi=2) return sớm */
    for (int r = 1; r <= rounds; r++) {
        uint64_t goal = currentTask->cpuTicks + WORKER_SLICE;
        uint64_t spins = 0;
        while (currentTask->cpuTicks < goal)  /* bận, không nhường: chỉ tick cắt được */
            spins++;
        kout_begin();
        serial_puts("round ");
        serial_putdec(r);
        serial_putc('/');
        serial_putdec(rounds);
        serial_puts(" done: own cpu ticks=");
        serial_putdec(currentTask->cpuTicks);
        serial_puts(" switched in ");
        serial_putdec(currentTask->switchesIn);
        serial_puts(" times, loop spins=");
        serial_putdec(spins);
        serial_putc('\n');
        kout_end();
    }
    kout_begin();
    serial_puts("returning -> taskKernelReturn() -> taskKill()\n");
    kout_end();
    __atomic_add_fetch(&workersDone, 1, __ATOMIC_SEQ_CST);
}

static void sleeper(uint64_t rdi) {
    hello_line(rdi);
    for (int r = 1; r <= 4; r++) {
        uint64_t t0 = timerTicks;
        taskSleepMs(50);
        uint64_t t1 = timerTicks;
        kout_begin();
        serial_puts("taskSleepMs(50) #");
        serial_putdec(r);
        serial_puts(": BLOCKED at t=");
        serial_putdec(t0);
        serial_puts(", running again at t=");
        serial_putdec(t1);
        serial_puts(" (+");
        serial_putdec(t1 - t0);
        serial_puts(" ms), switched in ");
        serial_putdec(currentTask->switchesIn);
        serial_puts(" times so far\n");
        kout_end();
    }
    sleeperDone = 1;
    for (;;)                                  /* ở lại, BLOCKED, để thấy trong bảng cuối */
        taskSleepMs(1000);
}

static void poller(uint64_t rdi) {
    hello_line(rdi);
    for (int r = 1; r <= 2; r++) {
        uint64_t t0 = timerTicks;
        uint64_t s0 = currentTask->switchesIn;
        sleep(50);                            /* cavOS sleep(): READY + handControl() */
        kout_begin();
        serial_puts("sleep(50) #");
        serial_putdec(r);
        serial_puts(": t=");
        serial_putdec(t0);
        serial_puts(" -> ");
        serial_putdec(timerTicks);
        serial_puts(", but was switched in ");
        serial_putdec(currentTask->switchesIn - s0);
        serial_puts(" times while 'sleeping' (stays READY)\n");
        kout_end();
    }
    pollerDone = 1;
    for (;;)
        taskSleepMs(1000);
}

/* "readHandler()" của cavOS (syscalls/io.c) + kbTaskRead() (drivers/kb.c), rút gọn:
 * chờ tới khi tty_q có ký tự. Chờ = state WAITING_INPUT; kbIrq đổi về READY. */
static void kbWaitInput(void) {
    while (tty_q.r == tty_q.w) {
#ifdef LOST_WAKEUP
        /* Bug CỐ Ý: kiểm tra "rỗng" rồi mới đặt WAITING_INPUT mà không cli, và khe hở được
         * kéo dài 300 ms (chỉ lần chờ đầu tiên). Phím tới trong khe: IRQ1 thấy task chưa
         * WAITING nên không đánh thức; task ngủ với một ký tự nằm sẵn trong ring. */
        static int gapDone;
        if (!gapDone) {
            gapDone = 1;
            kout_begin();
            serial_puts("LOST_WAKEUP: ring empty, 300 ms gap before WAITING_INPUT\n");
            kout_end();
            uint64_t until = timerTicks + 300;
            while (timerTicks < until)
                ;
        }
        kbWaiter = currentTask;
        currentTask->state = TASK_STATE_WAITING_INPUT;
#else
        __asm__ volatile("cli");              /* kiểm tra + đổi state là MỘT bước với IRQ1 */
        if (tty_q.r == tty_q.w) {
            kbWaiter = currentTask;
            currentTask->state = TASK_STATE_WAITING_INPUT;
        }
        __asm__ volatile("sti");
#endif
        while (currentTask->state == TASK_STATE_WAITING_INPUT)
            handControl();
    }
}

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void kbd(uint64_t rdi) {
    hello_line(rdi);
    char line[64];
    int  len = 0;
    uint64_t firstKey = 0;
    kout_begin();
    serial_puts("ready for input (type a line; \"quit\" ends the demo)\n");
    kout_end();

    for (;;) {
        uint64_t w0 = currentTask->wakeups;
        kbWaitInput();
        uint64_t woke = timerTicks;
        tty_char c;
        int n = 0;
        while (ring_get(&tty_q, &c)) {
            n++;
            kout_begin();
            serial_puts("key '");
            if (c.c == '\n') serial_puts("\\n");
            else serial_putc(c.c);
            serial_puts("' IRQ1 at t=");
            serial_putdec(c.tick);
            serial_puts(", task running at t=");
            serial_putdec(woke);
            serial_puts(" (");
            serial_putdec(woke - c.tick);
            serial_puts(" ms later)");
            if (currentTask->wakeups != w0)
                serial_puts(", woken by IRQ1");
            serial_putc('\n');
            kout_end();
            if (!len && c.c != '\n')
                firstKey = c.tick;
            if (c.c == '\n') {
                line[len] = 0;
                kout_begin();
                serial_puts("read() returns \"");
                serial_puts(line);
                serial_puts("\" (");
                serial_putdec(len);
                serial_puts(" chars, first key at t=");
                serial_putdec(firstKey);
                serial_puts(")\n");
                kout_end();
                if (streq(line, "quit")) {
                    kbdDone = 1;
                    return;                   /* thread này cũng thoát qua taskKernelReturn */
                }
                len = 0;
            } else if (c.c == '\b') {
                if (len) len--;
            } else if (len < 63) {
                line[len++] = c.c;
            }
        }
        if (n > 1) {
            kout_begin();
            serial_putdec(n);
            serial_puts(" keys were waiting in the ring at this wakeup\n");
            kout_end();
        }
    }
}

/* ------------------------------------------------------------ bảng task */
static void table_header(void) {
    serial_puts("  id  name     state          cpu ticks  switched in  wakeups  PML4       stack top PA\n");
}

static void table_row(Task *t) {
    serial_puts("  ");
    serial_putdec_right(t->id, 2);
    serial_puts("  ");
    serial_puts_pad(t->cmdline, 9);
    serial_puts_pad(taskStateName(t->state), 15);
    serial_putdec_right(t->cpuTicks, 9);
    serial_putdec_right(t->switchesIn, 13);
    serial_putdec_right(t->wakeups, 9);
    serial_puts("  ");
    serial_puthex_short(t->pagedir);
    serial_puts("  ");
    if (t->id == KERNEL_TASK_ID)
        serial_puts("(Limine boot stack)");
    else
        serial_puthex_short(t->stackPhys[USER_STACK_PAGES - 1]);
    serial_putc('\n');
}

static Task *spawn(void (*fn)(uint64_t), uint64_t arg, const char *name) {
    Task *t = taskCreateKernel((uint64_t)fn, arg);
    taskNameKernel(t, name);
    return t;
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    boot_init();

    serial_puts("=== Lab 0x07 - Multitasking & Scheduler ===\n\n");

    pmm_init();                                   /* Bài 5 */
    paging_init();                                /* Bài 6 */
    gdt_init();                                   /* Bài 7 */
    acpi_init();                                  /* Bài 8: chỉ cần MADT */
    isr_init();                                   /* Bài 9 + 10: IDT, APIC, sti */
    timer_init();                                 /* Bài 10, = initiateApicTimer() */
    input_init();                                 /* = fsMount("/dev/", CONNECTOR_DEV, 0, 0) */
    initiateKb();                                 /* Bài 11 */
    initiateMouse();
    serial_puts("[boot] pmm, paging, GDT/TSS, ACPI, IDT, APIC, timer 1 ms, PS/2 ready\n\n");

    uint64_t freeBefore = pmm_free_count();
    /* Tạo hết task trong cli: tick đầu tiên chỉ đổi task khi danh sách đã đủ, để bảng
     * dưới đây là trạng thái ban đầu thật. cavOS không làm vậy (tạo task với IF = 1). */
    __asm__ volatile("cli");
    initiateTasks();                              /* = initiateTasks() */
    initiateKernelThreads();                      /* = initiateKernelThreads() */
    Task *a = spawn(worker, 0, "A");
    spawn(worker, 1, "B");
    spawn(worker, 2, "C");
    spawn(sleeper, 3, "sleeper");
    spawn(poller, 4, "poller");
    spawn(kbd, 5, "kbd");

    int n = 0;
    for (Task *t = firstTask; t; t = t->next)
        n++;
    serial_puts("[tasks] ");
    serial_putdec(n);
    serial_puts(" tasks in the list; each new one = 1 PML4 + 4 stack pages + 1 TSS stack page"
                " + PDPT/PD/PT; pmm free ");
    serial_putdec(freeBefore);
    serial_puts(" -> ");
    serial_putdec(pmm_free_count());
    serial_puts("\n[tasks] task A first frame: RIP=");
    serial_puthex_short(a->registers.rip);
    serial_puts(" RSP=");
    serial_puthex_short(a->registers.usermode_rsp);
    serial_puts(" RFLAGS=");
    serial_puthex_short(a->registers.rflags);
    serial_puts(" CS=");
    serial_puthex_short(a->registers.cs);
    serial_puts(" SS=");
    serial_puthex_short(a->registers.usermode_ss);
    serial_puts(" RDI=");
    serial_putdec(a->registers.rdi);
    serial_puts(" [RSP]=");
    serial_puthex_short(*(uint64_t *)((uint8_t *)P2V(a->stackPhys[USER_STACK_PAGES - 1]) +
                                      PAGE_SIZE - 8));
    serial_puts(" (taskKernelReturn)\n");
    serial_puts("[tasks] list right after creation (firstTask -> next -> ...):\n");
    table_header();
    for (Task *t = firstTask; t; t = t->next)
        table_row(t);
    serial_puts("[tasks] sti: from now on every LAPIC tick calls schedule()\n");
    __asm__ volatile("sti");

    kout_begin();
    serial_puts("task 0 (kmain) blocks until all threads are done\n");
    kout_end();
    uint64_t start = timerTicks;
    while (!(workersDone == 3 && sleeperDone && pollerDone && kbdDone))
        taskSleepMs(20);
    while (reapedCount < 4)                       /* A, B, C, kbd đều đã return */
        taskSleepMs(5);

    __asm__ volatile("cli");                      /* chốt số liệu: từ đây không đổi task */
    serial_puts("\n[kernel] all threads done after ");
    serial_putdec(timerTicks - start);
    serial_puts(" ms. Per-task CPU ticks (1 tick = 1 ms on the LAPIC timer):\n");
    serial_puts("[kernel] tasks still in the list:\n");
    table_header();
    uint64_t sum = 0;
    for (Task *t = firstTask; t; t = t->next) {
        table_row(t);
        sum += t->cpuTicks;
    }
    serial_puts("[kernel] tasks already reaped (returned from their entry function):\n");
    for (int i = 0; i < reapedCount; i++) {
        ReapedInfo *r = &reaped[i];
        serial_puts("  ");
        serial_putdec_right(r->id, 2);
        serial_puts("  ");
        serial_puts_pad(r->name, 9);
        serial_puts_pad("reaped", 15);
        serial_putdec_right(r->cpuTicks, 9);
        serial_putdec_right(r->switchesIn, 13);
        serial_putdec_right(r->wakeups, 9);
        serial_puts("  at t=");
        serial_putdec(r->reapedAt);
        serial_putc('\n');
        sum += r->cpuTicks;
    }
    serial_puts("[kernel] sum of cpu ticks = ");
    serial_putdec(sum);
    serial_puts(", timerTicks = ");
    serial_putdec(timerTicks);
    serial_puts(" (ticks before initiateTasks() belong to nobody)\n");
    serial_puts("[kernel] pmm free now ");
    serial_putdec(pmm_free_count());
    serial_puts(" (before tasks: ");
    serial_putdec(freeBefore);
    serial_puts("); keys dropped: ");
    serial_putdec(tty_q.dropped);
    serial_puts("\n[kernel] done, cli; hlt\n");
    halt();
}
