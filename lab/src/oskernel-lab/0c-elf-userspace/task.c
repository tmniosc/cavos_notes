/* task.c — tạo / huỷ task, theo cavOS multitasking/task.c + stack.c (Bài 12).
 *
 * Đường đi giống cavOS:
 *   initiateTasks():   task 0 = chính luồng kmain đang chạy (không tạo stack mới cho nó),
 *                      rồi tạo dummyTask (idle) bằng taskCreateKernel().
 *   taskCreateKernel(rip, rdi) = taskCreate(id, rip, kernel, PageDirectoryAllocate())
 *                      + stackGenerateKernel(rdi) + taskCreateFinish() (CREATED -> READY).
 *   stackGenerateKernel: map stack ở USER_STACK_BOTTOM trong PML4 của task, rdi = tham số,
 *                      đẩy địa chỉ taskKernelReturn làm "địa chỉ trả về" -> hàm thread return
 *                      là rơi vào taskKill().
 *   taskKill():        giao task cho reaper (kernel_helper.c), đặt DEAD; nếu đang tự giết
 *                      mình thì sti + chờ tick kế tiếp đưa CPU đi chỗ khác.
 * Khác cavOS (ghi trên trang lab):
 *   - Task lấy từ pool tĩnh 16 ô (cavOS: malloc); stack 4 trang (cavOS: 2048).
 *   - Stack map chỉ RW (cavOS: PF_USER | PF_RW vì dùng chung code với task user).
 *   - Ghi địa chỉ trả về qua HHDM (cavOS: ChangePageDirectory sang PML4 của task rồi ghi).
 *   - Sửa danh sách bằng irq_save/irq_restore (cavOS: spinlock TASK_LL_MODIFY + cli/sti).
 *   - Task 0 không có syscall stack, không FPU state (chưa có userspace/SSE).
 */
#include "task.h"
#include "boot.h"
#include "gdt.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "spinlock.h"
#include "timer.h"

#define MAX_TASKS 16
static Task taskPool[MAX_TASKS];

Task *firstTask;
Task *volatile currentTask;
Task *dummyTask;
volatile int tasksInitiated;

static uint64_t taskIdCurr = 1;
static uint64_t taskGenerateId(void) { return taskIdCurr++; }

/* --------------------------------------------------------- tắt/mở ngắt */
static uint64_t irq_save(void) {
    uint64_t f;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(f) : : "memory");
    return f;
}
static void irq_restore(uint64_t f) {
    if (f & 0x200)
        __asm__ volatile("sti" : : : "memory");
}
static int interrupts_on(void) {
    uint64_t f;
    __asm__ volatile("pushfq; pop %0" : "=r"(f));
    return (f >> 9) & 1;
}

/* PMM của lab không có khoá: task 0 cấp frame trong lúc reaper trả frame
 * -> 2 lần đọc-sửa-ghi cùng một byte bitmap. Gói trong irq_save cho chắc. */
static uint64_t frame_alloc(void) {
    uint64_t f = irq_save();
    uint64_t pa = pmm_alloc();
    irq_restore(f);
    if (pa)
        memset8(P2V(pa), 0, PAGE_SIZE);
    return pa;
}

/* ------------------------------------------------------ danh sách task */
/* = cavOS taskListAllocate(): lấy một Task trống, nối vào CUỐI danh sách. */
static Task *taskListAllocate(void) {
    uint64_t f = irq_save();
    Task *target = 0;
    for (int i = 0; i < MAX_TASKS; i++)
        if (!taskPool[i].used) {
            target = &taskPool[i];
            break;
        }
    if (target) {
        memset(target, 0, sizeof(Task));     /* TASK_STATE_DEAD = 0, như cavOS */
        target->used = 1;
        Task *browse = firstTask;
        while (browse->next)
            browse = browse->next;
        browse->next = target;
    }
    irq_restore(f);
    return target;
}

/* = cavOS taskListDestroy(): không bao giờ là task đầu tiên. */
void taskListDestroy(Task *target) {
    uint64_t f = irq_save();
    Task *prev = firstTask;
    while (prev && prev->next != target)
        prev = prev->next;
    if (prev)
        prev->next = target->next;
    target->used = 0;
    irq_restore(f);
}

Task *taskGet(uint64_t id) {
    uint64_t f = irq_save();
    Task *browse = firstTask;
    while (browse && browse->id != id)
        browse = browse->next;
    irq_restore(f);
    return browse;
}

/* --------------------------------------------------------------- tạo */
static uint64_t tss_stack_alloc(void) {
    uint64_t pa = frame_alloc();             /* 1 trang: chỉ chứa khung iretq 176 byte */
    return (uint64_t)P2V(pa) + PAGE_SIZE;
}

/* = cavOS taskCreate() (phần kernel thread). */
static Task *taskCreate(uint64_t id, uint64_t rip, int kernel_task, uint64_t pagedir) {
    Task *target = taskListAllocate();
    if (!target) {
        serial_puts("[task] FATAL: task pool full\n");
        for (;;) __asm__ volatile("cli; hlt");
    }
    /* Khung iretq đầu tiên: CS/SS kernel, IF = 1, RIP = hàm thread. */
    target->registers.ds = GDT_KERNEL_DATA;
    target->registers.cs = GDT_KERNEL_CODE;
    target->registers.usermode_ss = GDT_KERNEL_DATA;
    target->registers.usermode_rsp = USER_STACK_BOTTOM;
    target->registers.rflags = 0x200;        /* "enable interrupts" */
    target->registers.rip = rip;

    target->id = id;
    target->kernel_task = kernel_task;
    target->state = TASK_STATE_CREATED;
    target->pagedir = pagedir;
    target->whileTssRsp = tss_stack_alloc();
    target->cmdline = "?";
    return target;
}

static void taskKernelReturn(void);

/* = cavOS stackGenerateKernel() + stackGenerateMutual(). */
static void stackGenerateKernel(Task *target, uint64_t parameter) {
    uint64_t base = USER_STACK_BOTTOM - USER_STACK_PAGES * PAGE_SIZE;
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint64_t pa = frame_alloc();
        target->stackPhys[i] = pa;
        vmap_in(target->pagedir, base + i * PAGE_SIZE, pa, PTE_RW, 0);
    }
    target->registers.rdi = parameter;       /* tham số thứ nhất (SysV ABI) */

    /* PUSH_TO_STACK(usermode_rsp, uint64_t, taskKernelReturn): VA này chỉ có trong PML4
     * của task mới, nên ghi qua HHDM vào trang cuối. RSP = 0x...FFF8 -> lúc vào hàm
     * RSP % 16 == 8, đúng như sau một lệnh call. */
    target->registers.usermode_rsp -= sizeof(uint64_t);
    uint64_t *slot = (uint64_t *)((uint8_t *)P2V(target->stackPhys[USER_STACK_PAGES - 1]) +
                                  PAGE_SIZE - sizeof(uint64_t));
    *slot = (uint64_t)taskKernelReturn;
}

void taskCreateFinish(Task *task) { task->state = TASK_STATE_READY; }

void taskNameKernel(Task *target, const char *str) { target->cmdline = str; }

Task *taskCreateKernel(uint64_t rip, uint64_t rdi) {
    Task *target = taskCreate(taskGenerateId(), rip, 1, paging_alloc_pagedir(firstTask->pagedir));
    stackGenerateKernel(target, rdi);
    taskCreateFinish(target);
    return target;
}

/* Lab 0x0a: = phần ring 3 của cavOS taskCreate() + stackGenerateUser(): CS/SS user (RPL 3),
 * code + stack map PTE_USER trong PML4 riêng, stack syscall riêng (whileSyscallRsp). */
Task *taskCreateUser(const void *code, uint64_t len, const char *name) {
    uint64_t pml4 = paging_alloc_pagedir(firstTask->pagedir);
    Task *t = taskCreateUserBlank(USER_CODE_VA, pml4, name);
    uint64_t codePa = frame_alloc();
    memcpy(P2V(codePa), code, len);
    vmap_in(pml4, USER_CODE_VA, codePa, PTE_USER, 0);          /* đọc + chạy, không ghi */
    return t;
}

/* Lab 0x0c: phần chung của mọi task ring 3 (cavOS taskCreate(..., kernel_task = false, ...)
 * + stackGenerateMutual): CS/SS user, stack user, stack syscall, FPU sạch. Không có code:
 * elfExecute() map các đoạn PT_LOAD vào pml4 trước, rồi gọi hàm này. */
Task *taskCreateUserBlank(uint64_t rip, uint64_t pml4, const char *name) {
    Task *t = taskCreate(taskGenerateId(), rip, 0, pml4);
    t->registers.cs = GDT_USER_CODE | 3;
    t->registers.usermode_ss = GDT_USER_DATA | 3;
    t->registers.ds = GDT_USER_DATA | 3;
    t->registers.rflags = 0x202;             /* IF = 1 */
    t->cmdline = name;

    uint64_t base = USER_STACK_BOTTOM - USER_STACK_PAGES * PAGE_SIZE;
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        t->stackPhys[i] = frame_alloc();
        vmap_in(pml4, base + i * PAGE_SIZE, t->stackPhys[i], PTE_USER | PTE_RW, 0);
    }
    t->registers.usermode_rsp = USER_STACK_BOTTOM;

    uint64_t sva = SYSCALL_STACK_VA + t->id * (SYSCALL_STACK_PAGES + 1) * PAGE_SIZE;
    for (int i = 0; i < SYSCALL_STACK_PAGES; i++) {
        t->syscallStackPhys[i] = frame_alloc();
        vmap(sva + i * PAGE_SIZE, t->syscallStackPhys[i], PTE_RW, 0);
    }
    t->whileSyscallRsp = sva + SYSCALL_STACK_PAGES * PAGE_SIZE;

    /* Lab 0x0b, = cavOS taskCreate(): FPU "sạch" cho task mới. FCW = 0x37f (mọi ngoại lệ x87
     * bị che, độ chính xác 64 bit), MXCSR = 0x1f80 (mọi ngoại lệ SSE bị che). Lab ghi MXCSR cả
     * vào offset 24 của vùng lưu (xrstor đọc từ đó); header XSAVE = 0 nghĩa là "trạng thái đầu". */
    memset(t->fpuenv, 0, sizeof(t->fpuenv));
    ((uint16_t *)t->fpuenv)[0] = 0x37f;
    *(uint32_t *)(t->fpuenv + 24) = 0x1f80;
    t->mxcsr = 0x1f80;
    /* CREATED: kmain gọi taskCreateFinish() sau khi in thông tin task */
    return t;
}

/* --------------------------------------------------------------- huỷ */
Task *volatile reaperTask;
static Spinlock LOCK_REAPER;

/* = cavOS taskCallReaper(): reaper chỉ giữ 1 task một lúc; đầy thì nhường CPU rồi thử lại. */
static void taskCallReaper(Task *target) {
    while (1) {
        spinlockAcquire(&LOCK_REAPER);
        if (!reaperTask) {
            reaperTask = target;
            spinlockRelease(&LOCK_REAPER);
            return;
        }
        spinlockRelease(&LOCK_REAPER);
        handControl();
    }
}

void taskKill(uint64_t id, uint16_t ret) {
    (void)ret;
    Task *task = taskGet(id);
    if (!task)
        return;
    taskCallReaper(task);                    /* reaper dọn sau, trong ngữ cảnh an toàn */
    task->state = TASK_STATE_DEAD;

    if (currentTask == task) {
        /* Như cavOS: không tự dọn được stack mình đang đứng -> chờ tick kế tiếp.
         * State = DEAD nên scheduler không bao giờ quay lại đây nữa. */
        __asm__ volatile("sti");
        while (1) {
        }
    }
}

/* = cavOS taskKernelReturn(): hàm thread return thì rơi vào đây. */
static void taskKernelReturn(void) {
    taskKill(currentTask->id, 0);
    while (1) {
    }
}

/* = phần "free stacks" của cavOS helperReaper() + PageDirectoryFree. */
void taskFreeResources(Task *target, int *frames, int *tables) {
    uint64_t f = irq_save();
    paging_free_pagedir(target->pagedir, firstTask->pagedir, frames, tables);
    (*tables)++;                             /* chính PML4 */
    pmm_free(V2P(target->whileTssRsp - PAGE_SIZE));
    (*frames)++;                             /* TSS stack */
    for (int i = 0; i < SYSCALL_STACK_PAGES; i++)
        if (target->syscallStackPhys[i]) {   /* Lab 0x0a: stack syscall (VA để lại, không dùng lại) */
            pmm_free(target->syscallStackPhys[i]);
            (*frames)++;
        }
    irq_restore(f);
}

/* ------------------------------------------------------------- nhường CPU */
/* = cavOS handControl() (cpu/system.c): đặt cờ rồi ĐỌC địa chỉ ma thuật chưa map
 * -> #PF -> handle_interrupt thấy cờ + CR2 khớp -> bỏ qua lệnh đọc -> schedule().
 * Lab viết lệnh đọc bằng asm để biết chắc nó dài 2 byte (8A 00 = movb (%rax),%al);
 * cavOS để C tự sinh rồi cộng RIP thêm 1 (xem Bài 12, Đọc thêm). */
void handControl(void) {
    if (!tasksInitiated)
        return;
    if (!interrupts_on()) {                  /* cavOS: assert(checkInterrupts()) */
        serial_puts("[task] handControl() called with interrupts off -> panic\n");
        for (;;) __asm__ volatile("cli; hlt");
    }
    currentTask->schedPageFault = 1;
    uint64_t a = SCHED_PAGE_FAULT_MAGIC_ADDRESS;
    __asm__ volatile("movb (%%rax), %%al" : "+a"(a) : : "memory");
}

/* = cavOS syscallNanosleep() (syscalls/linux/syscalls_clock.c): BLOCKED + giờ hẹn;
 * schedule() đổi lại READY khi timerTicks tới hẹn. Khác sleep() của timer.c (cavOS
 * cpu/timer.c): sleep() vẫn READY, chỉ nhường CPU rồi kiểm tra lại. */
void taskSleepMs(uint64_t ms) {
    currentTask->forcefulWakeupTimeUnsafe = timerTicks + ms;
    currentTask->state = TASK_STATE_BLOCKED;
    do
        handControl();
    while (currentTask->forcefulWakeupTimeUnsafe > timerTicks);
}

/* ------------------------------------------------------------- khởi tạo */
static void kernelDummyEntry(void) {
    /* cavOS: while (true) asm volatile("pause");  — lab dùng hlt để CPU nghỉ thật. */
    for (;;)
        __asm__ volatile("hlt");
}

void initiateTasks(void) {
    firstTask = &taskPool[0];
    memset(firstTask, 0, sizeof(Task));
    firstTask->used = 1;
    currentTask = firstTask;
    currentTask->id = KERNEL_TASK_ID;
    currentTask->state = TASK_STATE_READY;
    currentTask->pagedir = paging_read_cr3();          /* = GetPageDirectory() */
    currentTask->kernel_task = 1;
    currentTask->whileTssRsp = tss_stack_alloc();
    taskNameKernel(currentTask, "kernel");             /* entryCmdline */

    const uint64_t *pml4 = (const uint64_t *)P2V(currentTask->pagedir);
    serial_puts("[tasks] task 0 = this kmain execution, PML4 at ");
    serial_puthex_short(currentTask->pagedir);
    serial_puts(", PML4[255] (stack slot) = ");
    serial_puthex_short(pml4[255]);
    serial_puts(pml4[255] ? "  !! not empty: tasks would share stack tables\n" : " (empty)\n");
    serial_puts("[tasks] Current execution ready for multitasking\n");
    tasksInitiated = 1;

    /* task dummy: scheduler dùng khi không còn task READY nào */
    dummyTask = taskCreateKernel((uint64_t)kernelDummyEntry, 0);
    dummyTask->state = TASK_STATE_DUMMY;
    taskNameKernel(dummyTask, "dummy");
}

const char *taskStateName(uint8_t s) {
    switch (s) {
    case TASK_STATE_DEAD:          return "DEAD";
    case TASK_STATE_READY:         return "READY";
    case TASK_STATE_WAITING_INPUT: return "WAITING_INPUT";
    case TASK_STATE_CREATED:       return "CREATED";
    case TASK_STATE_BLOCKED:       return "BLOCKED";
    case TASK_STATE_DUMMY:         return "DUMMY";
    default:                       return "?";
    }
}
