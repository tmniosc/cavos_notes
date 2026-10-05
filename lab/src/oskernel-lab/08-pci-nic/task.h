/* task.h — task struct + danh sách task, theo cavOS include/task.h + multitasking/task.c (Bài 12).
 *
 * Giữ đúng thiết kế cavOS:
 *  - Task là một node trong DANH SÁCH LIÊN KẾT ĐƠN (firstTask -> ... -> NULL), không có
 *    run queue riêng: scheduler duyệt cả danh sách tìm task READY kế tiếp.
 *  - Ngữ cảnh của task = một khung AsmPassedInterrupt (đúng hình khung ngắt của isr_common).
 *    Đổi task = lưu khung của task cũ, dựng khung của task mới rồi iretq (schedule.c).
 *  - Mỗi task có PML4 riêng (copy PML4 của task 0) và stack chạy map ở CÙNG một VA
 *    (USER_STACK_BOTTOM) trong PML4 của nó; thêm 1 "TSS stack" (whileTssRsp) để dựng khung iretq.
 *  - Nhường CPU (handControl) = cố tình page fault ở địa chỉ "ma thuật".
 * Rút gọn: bỏ signal, file, fs, termios, FPU, pgid/sid, parent/children, syscall stack.
 * Thêm của lab: đếm tick CPU, số lần được chọn, số lần được đánh thức (in bảng cuối demo).
 */
#pragma once
#include <stdint.h>
#include "isr.h"

#define KERNEL_TASK_ID 0

/* cavOS include/paging.h: USER_STACK_PAGES = 2048 (8 MiB, cấp ngay lúc tạo task).
 * Lab: 4 trang = 16 KiB là đủ cho các thread demo. */
#define USER_STACK_PAGES  4
#define USER_STACK_BOTTOM 0x800000000000ULL          /* đỉnh nửa thấp, PML4 index 255 */
#define SCHED_PAGE_FAULT_MAGIC_ADDRESS 0x5FFFFFFFF000ULL

/* Giá trị y như enum TASK_STATE của cavOS (chỉ giữ những state lab dùng). */
typedef enum {
    TASK_STATE_DEAD          = 0,
    TASK_STATE_READY         = 1,
    TASK_STATE_WAITING_INPUT = 3,   /* chờ bàn phím: IRQ1 đổi về READY */
    TASK_STATE_CREATED       = 4,   /* vừa taskCreate(), chưa taskCreateFinish() */
    TASK_STATE_BLOCKED       = 8,   /* ngủ có hẹn giờ (forcefulWakeupTimeUnsafe) */
    TASK_STATE_DUMMY         = 69,  /* task "dummy" (idle): scheduler không bao giờ chọn bằng vòng tìm */
} TASK_STATE;

typedef struct Task Task;
struct Task {
    uint64_t id;
    volatile uint8_t state;
    int      kernel_task;                  /* lab chỉ có kernel thread */

    AsmPassedInterrupt registers;          /* ngữ cảnh đã lưu (khung ngắt) */
    uint64_t whileTssRsp;                  /* đỉnh "TSS stack": nạp vào TSS.rsp0 + chỗ dựng khung iretq */
    uint64_t pagedir;                      /* PA của PML4 riêng (cavOS: infoPd->pagedir, là VA) */

    volatile int      schedPageFault;      /* handControl() đặt 1 trước khi cố tình page fault */
    volatile uint64_t forcefulWakeupTimeUnsafe;   /* != 0: đánh thức khi timerTicks >= giá trị này */
    uint32_t tmpRecV;                      /* cavOS: số byte read() nhận được; lab: số ký tự */

    const char *cmdline;                   /* cavOS: tên task ("kernel", "dummy", "kernel" helper) */

    /* --- thêm của lab --- */
    volatile uint64_t cpuTicks;            /* số tick timer rơi vào lúc task này đang chạy */
    volatile uint64_t switchesIn;          /* số lần scheduler chuyển SANG task này */
    volatile uint64_t wakeups;             /* số lần được đánh thức (hết giờ ngủ / IRQ1) */
    uint64_t stackPhys[USER_STACK_PAGES];  /* PA từng trang stack (để in: cùng VA, khác PA) */
    int      used;                         /* ô trong pool đang dùng (cavOS: malloc) */

    Task *next;
};

extern Task *firstTask;
extern Task *volatile currentTask;
extern Task *dummyTask;
extern volatile int tasksInitiated;

void  initiateTasks(void);                               /* = cavOS initiateTasks() */
Task *taskCreateKernel(uint64_t rip, uint64_t rdi);      /* = cavOS taskCreateKernel() */
void  taskNameKernel(Task *target, const char *str);
void  taskCreateFinish(Task *task);                      /* CREATED -> READY */
Task *taskGet(uint64_t id);
void  taskKill(uint64_t id, uint16_t ret);
void  taskListDestroy(Task *target);                     /* gỡ khỏi danh sách + trả ô pool */
void  taskFreeResources(Task *target, int *frames, int *tables);   /* stack + PML4 + TSS stack */

void  handControl(void);                                 /* = cavOS handControl(): nhường CPU */
void  taskSleepMs(uint64_t ms);                          /* = cavOS syscallNanosleep(): BLOCKED */

const char *taskStateName(uint8_t s);

/* reaper (kernel_helper.c) */
extern Task *volatile reaperTask;
