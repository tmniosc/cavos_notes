/* syscall.c — SYSCALL/SYSRET như cavOS (Bài 16).
 *
 * Giống cavOS:
 *  - initiateSyscallInst(): CPUID 0x80000001 EDX bit 11, KERNEL_GS_BASE = &threadInfo,
 *    STAR[63:48] = GDT_USER_CODE - 16, STAR[47:32] = GDT_KERNEL_CODE, LSTAR = syscall_entry,
 *    EFER.SCE = 1, FMASK = IF | DF.
 *  - syscall_entry: chép từng lệnh của cavOS cpu/isr.asm: swapgs, dùng CR2 làm thanh ghi
 *    tạm, RSP = [gs:0] (stack syscall của task), dựng khung giống khung ngắt, gọi
 *    syscallHandler, cli, khôi phục, pop rsp, sysretq. KHÔNG swapgs khi về.
 *  - syscallHandler(): KERNEL_GS_BASE = &threadInfo, sti, tra bảng syscalls[rax], gọi với
 *    (rdi, rsi, rdx, r10, r8, r9), ghi kết quả vào rax. int 0x80 gọi cùng hàm này.
 * Khác cavOS: chỉ vài syscall (write, getpid, exit) + 4 syscall "báo cáo" của lab (440..443).
 */
#include "syscall.h"
#include "gdt.h"
#include "serial.h"
#include "kout.h"
#include "task.h"
#include "timer.h"

ThreadInfo threadInfo;
volatile uint64_t syscallCount, int80Count;

typedef uint64_t (*SyscallFn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static uint64_t syscalls[MAX_SYSCALLS];
static AsmPassedInterrupt *curRegs;      /* lab: syscall "báo cáo" cần nhìn khung */
static uint64_t curUserRsp;

#define ENOSYS 38
#define EFAULT 14

/* ------------------------------------------------------------------ entry */
__asm__(
    ".text\n"
    ".global syscall_entry\n"
    "syscall_entry:\n"
    "  swapgs\n"                         /* GS base <-> KERNEL_GS_BASE (= &threadInfo) */
    "  mov %rax, %cr2\n"                 /* use cr2 as an extra register */
    "  mov %gs:0, %rax\n"                /* threadInfo.syscall_stack */
    "  xchg %rax, %rsp\n"                /* switch stack ptrs */
    "  push %rax\n"                      /* the user's RSP */
    "  mov %cr2, %rax\n"
    "  push $0\n  push $0\n  push $0\n  push $0\n  push $0\n"   /* mimic ss, rsp, rflags, cs, rip */
    "  push $0\n"                        /* error */
    "  push $0\n"                        /* interrupt */
    "  push %rax\n  push %rbx\n  push %rcx\n  push %rdx\n"
    "  push %rsi\n  push %rdi\n  push %rbp\n"
    "  push %r8\n  push %r9\n  push %r10\n  push %r11\n"
    "  push %r12\n  push %r13\n  push %r14\n  push %r15\n"
    "  mov %ds, %rbp\n"
    "  push %rbp\n"
    "  mov %rsp, %rdi\n"
    "  call syscallHandler\n"
    "  cli\n"                            /* important to avoid race conditions */
    "  pop %rbp\n"
    "  mov %ebp, %ds\n"
    "  mov %ebp, %es\n"
    "  pop %r15\n  pop %r14\n  pop %r13\n  pop %r12\n"
    "  pop %r11\n  pop %r10\n  pop %r9\n  pop %r8\n"
    "  pop %rbp\n  pop %rdi\n  pop %rsi\n  pop %rdx\n"
    "  pop %rcx\n  pop %rbx\n  pop %rax\n"
    "  add $16, %rsp\n"                  /* error code and interrupt number */
    "  add $40, %rsp\n"                  /* the other interrupt stuff */
    "  pop %rsp\n"                       /* reset rsp */
    "  sysretq\n");

static void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}

static void hx(uint64_t v) { serial_puthex_short(v); }

void syscallDumpMsrs(const char *when) {
    uint64_t star = rdmsr(MSRID_STAR);
    serial_puts("[syscalls] MSRs ");
    serial_puts(when);
    serial_puts(": EFER ");
    hx(rdmsr(MSRID_EFER));
    serial_puts(" (SCE=");
    serial_putdec(rdmsr(MSRID_EFER) & 1);
    serial_puts(") STAR ");
    hx(star);
    serial_puts(" (SYSCALL CS=");
    hx((star >> 32) & 0xFFFF);
    serial_puts(" SS=");
    hx(((star >> 32) & 0xFFFF) + 8);
    serial_puts("; SYSRET CS=");
    hx((((star >> 48) & 0xFFFF) + 16) | 3);
    serial_puts(" SS=");
    hx((((star >> 48) & 0xFFFF) + 8) | 3);
    serial_puts(")\n[syscalls]      LSTAR ");
    hx(rdmsr(MSRID_LSTAR));
    serial_puts(" (syscall_entry = ");
    hx((uint64_t)syscall_entry);
    serial_puts(") FMASK ");
    hx(rdmsr(MSRID_FMASK));
    serial_puts(" (IF|DF) GS_BASE ");
    hx(rdmsr(MSRID_GSBASE));
    serial_puts(" KERNEL_GS_BASE ");
    hx(rdmsr(MSRID_KERNEL_GSBASE));
    serial_puts(" (&threadInfo = ");
    hx((uint64_t)&threadInfo);
    serial_puts(")\n");
}

void initiateSyscallInst(void) {
    uint32_t a, b, c, d;
    cpuid(0x80000001, &a, &b, &c, &d);
    serial_puts("[syscalls] CPUID 0x80000001 EDX = ");
    hx(d);
    serial_puts(", bit 11 (SYSCALL/SYSRET) = ");
    serial_putdec((d >> 11) & 1);
    serial_putc('\n');
    if (!((d >> 11) & 1)) {
        serial_puts("[syscalls] FATAL! No support for the syscall instruction found!\n");
        for (;;) __asm__ volatile("cli; hlt");
    }

    threadInfo.syscall_stack = 0;
    wrmsr(MSRID_KERNEL_GSBASE, (uint64_t)&threadInfo);

    uint64_t star = rdmsr(MSRID_STAR) & 0x00000000ffffffffULL;
    star |= ((uint64_t)GDT_USER_CODE - 16) << 48;
    star |= ((uint64_t)GDT_KERNEL_CODE) << 32;
    wrmsr(MSRID_STAR, star);
    wrmsr(MSRID_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSRID_EFER, rdmsr(MSRID_EFER) | 1);
    wrmsr(MSRID_FMASK, RFLAGS_IF | RFLAGS_DF);
    syscallDumpMsrs("after initiateSyscallInst()");
}

/* ------------------------------------------------------------------ handler */
static uint64_t entryGs, entryKgs;       /* lab: GS MSR ngay lúc vào, trước khi timer kịp đổi */

void syscallHandler(AsmPassedInterrupt *regs) {
    entryGs = rdmsr(MSRID_GSBASE);
    entryKgs = rdmsr(MSRID_KERNEL_GSBASE);
    wrmsr(MSRID_KERNEL_GSBASE, (uint64_t)&threadInfo);
    /* cavOS: rsp của user nằm ngay sau khung (syscall_entry đã push). Với int 0x80 chỗ đó
     * không phải của khung; lab lấy RSP từ khung ngắt trong trường hợp này. */
    uint64_t *rspPtr = (uint64_t *)((uint64_t)regs + sizeof(AsmPassedInterrupt));
    curUserRsp = regs->interrupt == 0x80 ? regs->usermode_rsp : *rspPtr;
    curRegs = regs;
    if (regs->interrupt == 0x80) int80Count++;
    else syscallCount++;

    /* cavOS: sti ở mọi đường vào ("do other task stuff while we're here!"). Lab chỉ sti cho
     * SYSCALL (đang đứng trên stack syscall riêng). int 0x80 đứng trên stack TSS; nếu timer
     * đổi task ở đây, lần chạy lại schedule() chép khung đã lưu lên ĐỈNH stack TSS, đè khung
     * int 0x80 của user. DEMO=-DINT80_STI làm y cavOS. */
#ifndef INT80_STI
    if (regs->interrupt != 0x80)
#endif
        __asm__ volatile("sti");

    uint64_t id = regs->rax;
    if (id >= MAX_SYSCALLS || !syscalls[id]) {
        kout_begin();
        serial_puts("[syscalls] no handler for syscall ");
        serial_putdec(id);
        serial_puts(" -> -ENOSYS\n");
        kout_end();
        regs->rax = (uint64_t)-ENOSYS;
        return;
    }
    regs->rax = ((SyscallFn)syscalls[id])(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
}

static void registerSyscall(uint32_t id, void *handler) { syscalls[id] = (uint64_t)handler; }

/* ------------------------------------------------------------------ syscall */
static uint64_t syscallWrite(uint64_t fd, uint64_t buf, uint64_t count) {
    if (fd != 1 && fd != 2) return (uint64_t)-9;          /* -EBADF */
    if (buf >= 0x800000000000ULL || buf + count > 0x800000000000ULL) return (uint64_t)-EFAULT;
    kout_begin();
    serial_puts("write(1): ");
    for (uint64_t i = 0; i < count; i++) serial_putc(((const char *)buf)[i]);
    kout_end();
    return count;
}

static uint64_t syscallGetPid(void) { return currentTask->id; }

static uint64_t syscallExitTask(uint64_t code) {
    kout_begin();
    serial_puts("exit(");
    serial_putdec(code);
    serial_puts(") -> taskKill\n");
    kout_end();
    currentTask->exitCode = (int)code;
    taskKill(currentTask->id, code);
    return 0;
}

/* 440: kernel nhìn thấy gì lúc vào. rdi/rsi/rdx = RSP, nhãn trả về, RFLAGS mà user đo trước */
static uint64_t labEntry(uint64_t uRsp, uint64_t uRet, uint64_t uFlags) {
    uint64_t rsp, cs, ss;
    __asm__ volatile("mov %%rsp, %0; mov %%cs, %1; mov %%ss, %2" : "=r"(rsp), "=r"(cs), "=r"(ss));
    kout_begin();
    serial_puts("in the kernel: CS="); hx(cs);
    serial_puts(" SS="); hx(ss);
    serial_puts(" RSP="); hx(rsp);
    serial_puts(" (syscall stack top "); hx(currentTask->whileSyscallRsp);
    serial_puts(")\n");
    kout_end();
    kout_begin();
    serial_puts("  RCX="); hx(curRegs->rcx);
    serial_puts(curRegs->rcx == uRet ? " = return address" : " != return address ");
    serial_puts(" | R11="); hx(curRegs->r11);
    serial_puts(" vs user RFLAGS "); hx(uFlags);
    serial_puts(" | saved user RSP="); hx(curUserRsp);
    serial_puts(curUserRsp == uRsp ? " (match)\n" : " (MISMATCH)\n");
    kout_end();
    kout_begin();
    serial_puts("  at entry, after swapgs: GS_BASE="); hx(entryGs);
    serial_puts(" KERNEL_GS_BASE="); hx(entryKgs);
    serial_puts(" (the user's GS); the handler then sets KERNEL_GS_BASE = &threadInfo\n");
    kout_end();
    return 0;
}

/* 441: user kể lại thanh ghi ngay sau sysret */
static uint64_t labAfter(uint64_t cs, uint64_t ss, uint64_t rcx, uint64_t r11) {
    kout_begin();
    serial_puts("back in ring 3 after sysret: CS="); hx(cs);
    serial_puts(" SS="); hx(ss);
    serial_puts(" RCX="); hx(rcx);
    serial_puts(" R11="); hx(r11);
    serial_puts("; GS_BASE seen by this syscall's entry "); hx(entryGs);
    serial_puts(" (= GS in ring 3 just before it)\n");
    kout_end();
    return 0;
}

/* 442: số chu kỳ rdtsc đo trong ring 3 */
static uint64_t labBench(uint64_t cycSyscall, uint64_t cycInt80, uint64_t n) {
    kout_begin();
    serial_puts("benchmark, "); serial_putdec(n);
    serial_puts(" x getpid: SYSCALL "); serial_putdec(cycSyscall);
    serial_puts(" TSC ticks ("); serial_putdec(cycSyscall / n);
    serial_puts(" per call), int 0x80 "); serial_putdec(cycInt80);
    serial_puts(" ("); serial_putdec(cycInt80 / n);
    serial_puts(" per call)\n");
    kout_end();
    return 0;
}

/* 443 (DEMO=-DBAD_SYSRET): như sigreturn của cavOS, RCX lấy từ chỗ user đặt, không kiểm */
static uint64_t labBadRet(uint64_t newRip) {
    kout_begin();
    serial_puts("setting the return RIP (RCX) to "); hx(newRip);
    serial_puts(" without a canonical check, then sysret\n");
    kout_end();
    curRegs->rcx = newRip;
    return 0;
}

void initiateSyscalls(void) {
    registerSyscall(1, syscallWrite);
    registerSyscall(39, syscallGetPid);
    registerSyscall(60, syscallExitTask);
    serial_puts("[syscalls] System calls are ready to fire: 1 write, 39 getpid, 60 exit"
                " + lab 440..443\n");
    registerSyscall(440, labEntry);
    registerSyscall(441, labAfter);
    registerSyscall(442, labBench);
    registerSyscall(443, labBadRet);
}
