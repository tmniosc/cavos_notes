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
#include "syscall.h"
#include "serial.h"
#include "sse.h"

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
    threadInfo.syscall_stack = next->whileSyscallRsp;      /* Lab 0x0a, như cavOS */

    /* Apply new MSRIDs (cavOS: không lưu lại FS/GS của task cũ, chỉ nạp của task mới) */
    wrmsr(MSRID_FSBASE, next->fsbase);
    wrmsr(MSRID_GSBASE, next->gsbase);
    wrmsr(MSRID_KERNEL_GSBASE, (uint64_t)&threadInfo);

    /* Save generic (and non) registers */
    memcpy(&old->registers, cpu, sizeof(AsmPassedInterrupt));

    /* Lab 0x0b = cavOS "Save & load appropriate FPU state": chỉ task user (kernel build
     * -mno-sse, không đụng FPU). cavOS dùng fxsave/fxrstor: x87 + XMM0-15 + MXCSR, 512 byte.
     * DEMO=-DXSAVE: xsave/xrstor với mặt nạ = XCR0 (thêm nửa cao YMM).
     * DEMO=-DNO_FPU_SAVE: không lưu gì. */
#ifndef NO_FPU_SAVE
    /* lab: trước initiateSSE() (CR4.OSFXSR = 0), ldmxcsr/xrstor là #UD. cavOS không cần kiểm vì
     * không có task user nào chạy trước initiateSSE(); lab chạy một chương trình thử ở đó. */
    if (sseReady && !old->kernel_task) {
#ifdef XSAVE
        __asm__ volatile("xsave %0" : "+m"(old->fpuenv) : "a"(7), "d"(0));
#else
        __asm__ volatile("fxsave %0" : "=m"(old->fpuenv));
        __asm__ volatile("stmxcsr %0" : "=m"(old->mxcsr));
#endif
    }
    if (sseReady && !next->kernel_task) {
#ifdef XSAVE
        __asm__ volatile("xrstor %0" : : "m"(next->fpuenv), "a"(7), "d"(0));
#else
        __asm__ volatile("fxrstor %0" : : "m"(next->fpuenv));
        __asm__ volatile("ldmxcsr %0" : : "m"(next->mxcsr));
#endif
    }
#endif

    /* Lab 0x0a: task kế tiếp bị cắt khi đang ở ring 0 TRÊN chính stack TSS của nó (vd trong
     * handler int 0x80 đã sti): bước chép dưới đây sẽ đè lên khung ngắt đang nằm ở đỉnh stack đó */
    if (!(next->registers.cs & 3) && next->whileTssRsp && next->exitCode >= 0 &&   /* bỏ qua task đang bị
                                                         giết vì lỗi CPU: khung đó không bao giờ dùng lại */
        next->registers.usermode_rsp > next->whileTssRsp - PAGE_SIZE &&
        next->registers.usermode_rsp <= next->whileTssRsp) {
        static int warned;
        if (!warned++) {
            serial_puts("\n[sched] task ");
            serial_putdec(next->id);
            serial_puts(" was preempted in ring 0 on its own TSS stack (RSP ");
            serial_puthex_short(next->registers.usermode_rsp);
            serial_puts(", TSS stack top ");
            serial_puthex_short(next->whileTssRsp);
            serial_puts("): copying its frame to the top overwrites the frame of the interrupt it was handling\n");
        }
    }

    /* Put next task's registers in tssRsp */
    AsmPassedInterrupt *iretqRsp =
        (AsmPassedInterrupt *)(next->whileTssRsp - sizeof(AsmPassedInterrupt));
    memcpy(iretqRsp, &next->registers, sizeof(AsmPassedInterrupt));

    asm_finalize((uint64_t)iretqRsp, next->pagedir);
}
