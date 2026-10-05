/* syscall.h — lệnh SYSCALL/SYSRET theo cavOS cpu/fastSyscall.c + cpu/isr.asm (syscall_entry)
 * + syscalls/syscalls.c (bảng syscall, syscallHandler). Bài 16.
 */
#pragma once
#include <stdint.h>
#include "isr.h"

#define MSRID_FSBASE        0xC0000100
#define MSRID_GSBASE        0xC0000101
#define MSRID_KERNEL_GSBASE 0xC0000102
#define MSRID_EFER          0xC0000080
#define MSRID_STAR          0xC0000081
#define MSRID_LSTAR         0xC0000082
#define MSRID_FMASK         0xC0000084
#define RFLAGS_IF (1u << 9)
#define RFLAGS_DF (1u << 10)

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t v) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}

/* = cavOS include/system.h: syscall_entry đọc %gs:0 = stack syscall của task đang chạy */
typedef struct ThreadInfo {
    uint64_t syscall_stack;
    uint64_t lapic_id;
} ThreadInfo;
extern ThreadInfo threadInfo;

#define MAX_SYSCALLS 450
void initiateSyscallInst(void);          /* = cavOS: CPUID, threadInfo, STAR/LSTAR/EFER.SCE/FMASK */
void initiateSyscalls(void);             /* = cavOS: registerSyscall(...) cho từng số */
void syscallHandler(AsmPassedInterrupt *regs);
void syscallDumpMsrs(const char *when);
extern void syscall_entry(void);
extern volatile uint64_t syscallCount, int80Count;
