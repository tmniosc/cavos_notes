/* isr.h — khung thanh ghi mà stub asm để lại trên stack, giống cavOS include/isr.h. */
#pragma once
#include <stdint.h>

/* Thứ tự = ngược thứ tự push trong isr_common (thấp -> cao địa chỉ). */
typedef struct {
    uint64_t ds;                   /* isr_common push cuối cùng */

    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    uint64_t interrupt;            /* stub push số vector */
    uint64_t error;                /* CPU push (8,10-14,17,21,29,30) hoặc stub push 0 */

    uint64_t rip;                  /* 5 qword CPU tự push */
    uint64_t cs;
    uint64_t rflags;
    uint64_t usermode_rsp;
    uint64_t usermode_ss;
} AsmPassedInterrupt;

typedef void (*FunctionPtr)(AsmPassedInterrupt *regs);

void isr_init(void);               /* = cavOS initiateISR() (chưa có APIC, không sti) */
void register_irq_handler(uint8_t vector, FunctionPtr handler);

/* Lab: "lỗi có chủ đích" — handler báo cáo rồi cộng skip vào RIP để chạy tiếp. */
void isr_expect(uint64_t vector_mask, uint8_t skip);
int  isr_expect_hit(void);

void pic_dump(void);
