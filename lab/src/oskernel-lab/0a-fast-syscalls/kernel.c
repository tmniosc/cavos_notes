/* Lab 0x0a — Fast Syscalls (Bài 16).
 * Thứ tự giống cavOS _start():
 *   ... initiateTasks -> initiateKernelThreads -> initiateNetworking -> initiatePCI
 *   -> fsMount("/"), fsMount("/boot/")          (Lab 0x09)
 *   -> initiateSyscallInst()   STAR / LSTAR / EFER.SCE / FMASK, KERNEL_GS_BASE = &threadInfo
 *   -> initiateSyscalls()      bảng syscalls[]
 * Rồi chạy 3 chương trình ring 3 nhỏ (user.S), mỗi cái một task có PML4 riêng:
 *   main: write, getpid, xem thanh ghi lúc vào/ra, đo 10000 SYSCALL và 10000 int 0x80, exit(7)
 *   gs0 : đọc %gs:0 trước mọi syscall;  gs1: đọc %gs:0 ngay sau một syscall
 * DEMO: -DBAD_SYSRET chạy thêm chương trình làm RCX không canonical trước sysret.
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

LIMINE_BASE_REVISION(2)

extern const uint8_t user_main_start[], user_main_end[], user_gs0_start[], user_gs0_end[];
extern const uint8_t user_gs1_start[], user_gs1_end[], user_badret_start[], user_badret_end[];

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static void runUser(const char *name, const uint8_t *start, const uint8_t *end) {
    kout_begin_raw();                            /* task mới chạy ngay: giữ khoá dòng in tới hết dòng */
    Task *t = taskCreateUser(start, end - start, name);
    uint64_t id = t->id;
    serial_puts("\n[kernel] task ");
    serial_putdec(id);
    serial_puts(" (");
    serial_puts(name);
    serial_puts("): ");
    serial_putdec(end - start);
    serial_puts(" bytes of code at VA ");
    serial_puthex_short(USER_CODE_VA);
    serial_puts(", CS=");
    serial_puthex_short(t->registers.cs);
    serial_puts(" SS=");
    serial_puthex_short(t->registers.usermode_ss);
    serial_puts(" RSP=");
    serial_puthex_short(t->registers.usermode_rsp);
    serial_puts(", syscall stack top ");
    serial_puthex_short(t->whileSyscallRsp);
    serial_putc('\n');
    kout_end();
    taskCreateFinish(t);                         /* CREATED -> READY: từ đây scheduler chọn được */
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
    while (taskGet(id))                          /* chờ reaper dọn */
        taskSleepMs(5);
    taskSleepMs(20);                             /* để dòng in của reaper xong hẳn */
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();
    serial_init();
    boot_init();
    serial_puts("=== Lab 0x0a - Fast Syscalls ===\n\n");
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

    syscallDumpMsrs("before");
    initiateSyscallInst();                        /* Bài 16 */
    initiateSyscalls();
    uint64_t *pml4 = (uint64_t *)P2V(firstTask->pagedir);
    serial_puts("[kernel] model PML4[1] (user code slot) = ");
    serial_puthex_short(pml4[1]);
    serial_puts(", PML4[255] (stack slot) = ");
    serial_puthex_short(pml4[255]);
    serial_puts(", PML4[509] (syscall stacks, shared) = ");
    serial_puthex_short(pml4[509]);
    serial_putc('\n');

    runUser("main", user_main_start, user_main_end);
    runUser("gs0", user_gs0_start, user_gs0_end);
    runUser("gs1", user_gs1_start, user_gs1_end);
#ifdef BAD_SYSRET
    runUser("badret", user_badret_start, user_badret_end);
#endif

    serial_puts("\n[kernel] SYSCALL entries ");
    serial_putdec(syscallCount);
    serial_puts(", int 0x80 entries ");
    serial_putdec(int80Count);
    serial_putc('\n');
    syscallDumpMsrs("at the end (task 0 running)");
    serial_puts("[kernel] done, cli; hlt\n");
    halt();
}
