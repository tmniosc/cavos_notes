/* schedule.c — = cavOS multitasking/schedule.c ("Clock-tick triggered scheduler"), Bài 12.
 *
 * schedule(rsp) được gọi TỪ BÊN TRONG một ngắt: timerTick (mỗi 1 ms) hoặc page fault
 * ma thuật của handControl(). rsp trỏ tới khung AsmPassedInterrupt mà isr_common vừa
 * đẩy lên stack của task đang chạy. Hàm này KHÔNG return:
 *   1. tìm task READY kế tiếp sau currentTask (vòng tròn; đánh thức task hết giờ ngủ);
 *      không có thì dùng dummyTask;
 *   2. TSS.rsp0 = whileTssRsp của task mới;
 *   3. chép khung ngắt hiện tại vào old->registers (đây là "lưu ngữ cảnh");
 *   4. chép next->registers xuống đỉnh TSS stack của task mới;
 *   5. asm_finalize(khung đó, CR3 mới): mov rsp, mov cr3, pop 15 thanh ghi, iretq.
 * Bỏ so với cavOS: signal, itimer, spinlockQueueEntry, FS/GS base MSR, fxsave/fxrstor.
 * Thêm của lab: đếm switchesIn / wakeups.
 */
#include "schedule.h"
#include "task.h"
#include "gdt.h"
#include "timer.h"
#include "boot.h"

extern void asm_finalize(uint64_t rsp, uint64_t cr3);   /* isr.c (cavOS: cpu/isr.asm) */

void schedule(uint64_t rsp) {
    if (!tasksInitiated)
        return;

    AsmPassedInterrupt *cpu = (AsmPassedInterrupt *)rsp;
    Task *next = currentTask->next;
    if (!next)
        next = firstTask;

    int fullRun = 0;
    while (next->state != TASK_STATE_READY) {
        if (next->forcefulWakeupTimeUnsafe && next->forcefulWakeupTimeUnsafe <= timerTicks) {
            /* "no race! the task has to already have been suspended to end up here" */
            next->state = TASK_STATE_READY;
            next->forcefulWakeupTimeUnsafe = 0;
            next->wakeups++;
            break;
        }
        next = next->next;
        if (!next) {
            fullRun++;
            if (fullRun > 2)
                break;
            next = firstTask;
        }
    }

    /* found no task */
    if (!next)
        next = dummyTask;

    Task *old = currentTask;
    currentTask = next;
    if (next != old)
        next->switchesIn++;

    /* Change TSS rsp0 (software multitasking) */
    tssPtr->rsp0 = next->whileTssRsp;

    /* Save generic (and non) registers */
    memcpy(&old->registers, cpu, sizeof(AsmPassedInterrupt));

    /* Put next task's registers in tssRsp */
    AsmPassedInterrupt *iretqRsp =
        (AsmPassedInterrupt *)(next->whileTssRsp - sizeof(AsmPassedInterrupt));
    memcpy(iretqRsp, &next->registers, sizeof(AsmPassedInterrupt));

    asm_finalize((uint64_t)iretqRsp, next->pagedir);
}
