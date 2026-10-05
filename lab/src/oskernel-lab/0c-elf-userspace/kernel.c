/* Lab 0x0c — Userspace & ELF Loader (Bài 18).
 * Thứ tự giống cavOS _start():
 *   ... initiatePCI -> fsMount("/"), fsMount("/boot/") -> initiateSyscallInst -> initiateSyscalls
 *   -> initiateSSE() -> run("/bin/hello", ...)
 * run() ở đây = cavOS utilities/shell/shell.c run(): elfExecute() rồi taskCreateFinish() và chờ.
 * /bin/hello là ELF tĩnh build từ user/hello.c, nằm trên phân vùng ext2 (Lab 0x09).
 * Chạy hai lần với số tham số khác nhau để thấy RSP lúc vào phụ thuộc argc + envc.
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
#include "elf.h"

LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

/* = cavOS run(binary, wait = true, argc, argv) */
static void run(const char *path, uint32_t argc, const char **argv, uint32_t envc, const char **envv) {
    kout_begin_raw();
    serial_puts("\n[kernel] run(\"");
    serial_puts(path);
    serial_puts("\"), argc ");
    serial_putdec(argc);
    serial_puts(", envc ");
    serial_putdec(envc);
    serial_putc('\n');
    Task *t = elfExecute(path, argc, argv, envc, envv);
    kout_end();
    if (!t) {
        serial_puts("[kernel] elfExecute failed\n");
        return;
    }
    uint64_t id = t->id;
    taskCreateFinish(t);
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

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();
    serial_init();
    boot_init();
    serial_puts("=== Lab 0x0c - Userspace & ELF Loader ===\n\n");
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
    initiateSSE();                                /* Bài 17 */
    serial_puts("[boot] syscalls and SSE ready\n");
    fsListDir("/bin");

    const char *argv1[] = {"/bin/hello", "first", "second arg"};
    const char *envv[] = {"HOME=/", "LAB=0x0c"};
    run("/bin/hello", 3, argv1, 2, envv);         /* Bài 18 */

    const char *argv2[] = {"/bin/hello", "one"};
    run("/bin/hello", 2, argv2, 2, envv);

    run("/bin/missing", 1, argv2, 0, 0);

    serial_puts("\n[kernel] done, cli; hlt\n");
    halt();
}
