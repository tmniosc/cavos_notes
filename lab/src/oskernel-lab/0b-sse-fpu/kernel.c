/* Lab 0x0b — SSE & FPU (Bài 17).
 * Thứ tự giống cavOS _start():
 *   ... initiatePCI -> fsMount -> initiateSyscallInst -> initiateSyscalls (Lab 0x0a)
 *   -> initiateSSE()   CR0.EM = 0, CR0.MP = 1, CR4.OSFXSR, CR4.OSXMMEXCPT, fninit, CR0.NE,
 *                      CR4.OSXSAVE + XCR0 = 7 nếu CPU có XSAVE/AVX
 * Rồi:
 *   probe trước và sau initiateSSE(): một lệnh SSE và một lệnh AVX trong ring 3;
 *   fpu_A và fpu_B CHẠY CÙNG LÚC: mỗi task giữ một mẫu trong YMM0 và ST(0), bị timer đổi qua
 *   lại, rồi đếm số lần mẫu của mình bị thay. Scheduler lưu FPU như cavOS (fxsave/fxrstor).
 * DEMO: -DXSAVE (xsave/xrstor, lưu cả nửa cao YMM), -DNO_FPU_SAVE (không lưu gì).
 * QEMU cần -cpu max để có AVX trong TCG.
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
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "serial.h"
#include "task.h"
#include "kernel_helper.h"
#include "pci.h"
#include "nic.h"
#include "ahci.h"
#include "vfs.h"
#include "syscall.h"
#include "kout.h"
#include "sse.h"

LIMINE_BASE_REVISION(2)

extern const uint8_t user_fpuA_start[], user_fpuA_end[], user_fpuB_start[], user_fpuB_end[];
extern const uint8_t user_probe_start[], user_probe_end[];

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static Task *spawn(const char *name, const uint8_t *start, const uint8_t *end) {
    Task *t = taskCreateUser(start, end - start, name);
    serial_puts("[kernel] task ");
    serial_putdec(t->id);
    serial_puts(" (");
    serial_puts(name);
    serial_puts("): ");
    serial_putdec(end - start);
    serial_puts(" bytes of code, FPU area at ");
    serial_puthex_short((uint64_t)t->fpuenv);
    serial_puts(" (FCW 0x37f, MXCSR 0x1f80)\n");
    return t;
}

static void waitDead(Task *t) {
    uint64_t id = t->id;
    while (t->state != TASK_STATE_DEAD)
        taskSleepMs(5);
    kout_begin_raw();
    serial_puts("[kernel] task ");
    serial_putdec(id);
    serial_puts(" ended, exit code ");
    if (t->exitCode < 0) {
        serial_puts("-");
        serial_putdec(-t->exitCode);
        serial_puts(" (killed by CPU exception)\n");
    } else {
        serial_putdec(t->exitCode);
        serial_putc('\n');
    }
    kout_end();
    while (taskGet(id))
        taskSleepMs(5);
    taskSleepMs(20);
}

static void runOne(const char *name, const uint8_t *start, const uint8_t *end) {
    kout_begin_raw();
    serial_putc('\n');
    Task *t = spawn(name, start, end);
    kout_end();
    taskCreateFinish(t);
    waitDead(t);
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();
    serial_init();
    boot_init();
    serial_puts("=== Lab 0x0b - SSE & FPU ===\n\n");
    pmm_init();                                   /* Bài 5 */
    paging_init();                                /* Bài 6 */
    gdt_init();                                   /* Bài 7 */
    acpi_init();                                  /* Bài 8 */
    isr_init();                                   /* Bài 9 + 10 */
    timer_init();                                 /* Bài 10 */
    input_init();
    initiateKb();                                 /* Bài 11 */
    initiateMouse();
    initiateTasks();                              /* Bài 12 */
    initiateKernelThreads();
    serial_puts("[boot] pmm, paging, GDT/TSS, ACPI, IDT, APIC, timer, PS/2, tasks ready\n\n");
    initiateNetworking();                         /* Bài 13 */
    initiatePCI();                                /* Bài 14 + 15 */
    fsMount("/", 0, 1);                           /* Bài 15 */
    fsMount("/boot/", 0, 0);
    serial_putc('\n');
    initiateSyscallInst();                        /* Bài 16 */
    initiateSyscalls();
    serial_putc('\n');

#if defined(XSAVE)
    serial_puts("[kernel] FPU switch mode: xsave/xrstor with mask XCR0 = 7 (DEMO=-DXSAVE)\n");
#elif defined(NO_FPU_SAVE)
    serial_puts("[kernel] FPU switch mode: none (DEMO=-DNO_FPU_SAVE)\n");
#else
    serial_puts("[kernel] FPU switch mode: fxsave/fxrstor + stmxcsr/ldmxcsr, like cavOS\n");
#endif
    sseDump("left by Limine");
    runOne("probe", user_probe_start, user_probe_end);

    serial_putc('\n');
    initiateSSE();                                /* Bài 17 */
    sseDump("after initiateSSE()");
    runOne("probe", user_probe_start, user_probe_end);

    /* hai task chạy cùng lúc: timer đổi qua lại giữa chúng */
    kout_begin_raw();
    serial_putc('\n');
    Task *a = spawn("fpu_A", user_fpuA_start, user_fpuA_end);
    Task *b = spawn("fpu_B", user_fpuB_start, user_fpuB_end);
    kout_end();
    taskCreateFinish(a);
    taskCreateFinish(b);
    waitDead(a);
    waitDead(b);

    serial_puts("\n[kernel] done, cli; hlt\n");
    halt();
}
