/* isr.c — stub ngắt + handle_interrupt, theo cavOS cpu/isr.asm + cpu/isr.c (Bài 9).
 *
 * cavOS viết stub bằng NASM (isr.asm). Lab viết cùng logic bằng GNU as (AT&T) trong
 * khối asm toàn cục để khỏi sửa common.mk. Khác cavOS ở 3 chỗ, có ghi trên trang lab:
 *  1. Vector 17, 21 (Intel SDM Vol.3 bảng 6-1) và 29, 30 (AMD APM) dùng stub CÓ error code;
 *     cavOS coi là không có.
 *  2. Căn RSP 16 byte trước "call handle_interrupt" (cavOS gọi khi RSP lệch 8).
 *  3. Cổng #DF dùng IST1 (cavOS: ist = 0 cho mọi cổng).
 * Lab 0x05: thêm EOI cho Local APIC, đếm ngắt spurious, và isr_init() gọi apic_init() + sti
 * ở cuối như cavOS initiateISR().
 * Lab 0x07: thêm asm_finalize (cavOS isr.asm) và nhánh "page fault ma thuật" của handControl()
 * trong handle_interrupt -> schedule().
 * Lab 0x08: DEMO=-DEOI_LATE gửi EOI sau handler cho IRQ PCI (xem handle_interrupt).
 */
#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include "io.h"
#include "apic.h"
#include "serial.h"
#include "task.h"
#include "schedule.h"

/* ------------------------------------------------------------------ stub asm */
__asm__(
    ".text\n"
    /* Khung chung: lưu 15 thanh ghi + DS, nạp DS/ES/SS kernel, gọi C, khôi phục, iretq. */
    "isr_common:\n"
    "  push %rax\n  push %rbx\n  push %rcx\n  push %rdx\n"
    "  push %rsi\n  push %rdi\n  push %rbp\n"
    "  push %r8\n  push %r9\n  push %r10\n  push %r11\n"
    "  push %r12\n  push %r13\n  push %r14\n  push %r15\n"
    "  mov %ds, %rbp\n"
    "  push %rbp\n"
    "  mov $0x30, %bx\n"
    "  mov %bx, %ds\n  mov %bx, %es\n  mov %bx, %ss\n"
    "  mov %rsp, %rdi\n"                 /* arg 1 = con trỏ tới AsmPassedInterrupt */
    "  sub $8, %rsp\n"                   /* 23 qword đã push -> lệch 8, căn lại 16 */
    "  call handle_interrupt\n"
    "  add $8, %rsp\n"
    ".global asm_isr_exit\n"
    "asm_isr_exit:\n"
    "  pop %rbp\n"
    "  mov %ebp, %es\n"                  /* như cavOS: chỉ khôi phục ES */
    "  pop %r15\n  pop %r14\n  pop %r13\n  pop %r12\n"
    "  pop %r11\n  pop %r10\n  pop %r9\n  pop %r8\n"
    "  pop %rbp\n  pop %rdi\n  pop %rsi\n  pop %rdx\n"
    "  pop %rcx\n  pop %rbx\n  pop %rax\n"
    "  add $16, %rsp\n"                  /* bỏ vector + error code */
    "  iretq\n"

    /* = cavOS asm_finalize (cpu/isr.asm): rdi = khung iretq của task mới (trên TSS stack
     * của nó), rsi = CR3 mới. Đổi stack TRƯỚC, đổi CR3 SAU (khung nằm ở nửa cao, PML4 nào
     * cũng thấy), rồi chạy đúng đoạn thoát của isr_common. cavOS chép lại nguyên đoạn pop;
     * lab nhảy vào asm_isr_exit (giống hệt). */
    ".global asm_finalize\n"
    "asm_finalize:\n"
    "  mov %rdi, %rsp\n"
    "  mov %rsi, %cr3\n"
    "  jmp asm_isr_exit\n"

    ".macro ISR_ERR n\n"                 /* CPU đã push error code */
    "isr\\n:\n"
    "  push $\\n\n"
    "  jmp isr_common\n"
    ".endm\n"
    ".macro ISR_NOERR n\n"               /* push 0 giả để khung luôn cùng hình */
    "isr\\n:\n"
    "  push $0\n"
    "  push $\\n\n"
    "  jmp isr_common\n"
    ".endm\n"

    "ISR_NOERR 0\n  ISR_NOERR 1\n  ISR_NOERR 2\n  ISR_NOERR 3\n"
    "ISR_NOERR 4\n  ISR_NOERR 5\n  ISR_NOERR 6\n  ISR_NOERR 7\n"
    "ISR_ERR 8\n    ISR_NOERR 9\n  ISR_ERR 10\n   ISR_ERR 11\n"
    "ISR_ERR 12\n   ISR_ERR 13\n   ISR_ERR 14\n   ISR_NOERR 15\n"
    "ISR_NOERR 16\n ISR_ERR 17\n   ISR_NOERR 18\n ISR_NOERR 19\n"
    "ISR_NOERR 20\n ISR_ERR 21\n   ISR_NOERR 22\n ISR_NOERR 23\n"
    "ISR_NOERR 24\n ISR_NOERR 25\n ISR_NOERR 26\n ISR_NOERR 27\n"
    "ISR_NOERR 28\n ISR_ERR 29\n   ISR_ERR 30\n   ISR_NOERR 31\n"
    /* IRQ 0-15 của PIC sau khi remap -> vector 32-47 */
    "ISR_NOERR 32\n ISR_NOERR 33\n ISR_NOERR 34\n ISR_NOERR 35\n"
    "ISR_NOERR 36\n ISR_NOERR 37\n ISR_NOERR 38\n ISR_NOERR 39\n"
    "ISR_NOERR 40\n ISR_NOERR 41\n ISR_NOERR 42\n ISR_NOERR 43\n"
    "ISR_NOERR 44\n ISR_NOERR 45\n ISR_NOERR 46\n ISR_NOERR 47\n"
    "ISR_NOERR 128\n"                    /* int 0x80 = syscall kiểu cũ */
    "ISR_NOERR 255\n"                    /* APIC spurious */

    ".section .data\n"
    ".global asm_isr_redirect_table\n"
    "asm_isr_redirect_table:\n"
    ".irp i,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,"
    "24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47\n"
    "  .quad isr\\i\n"
    ".endr\n"
    ".text\n");

extern void *asm_isr_redirect_table[];
extern void  isr128(void);
extern void  isr255(void);

/* ------------------------------------------------------------- tên exception */
/* Chuỗi y hệt mảng exceptions[] trong cavOS cpu/isr.c; thêm mnemonic SDM. */
static const char *exceptions[32] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved"};

static const char *mnemonic[32] = {
    "#DE", "#DB", "NMI", "#BP", "#OF", "#BR", "#UD", "#NM",
    "#DF", "---", "#TS", "#NP", "#SS", "#GP", "#PF", "---",
    "#MF", "#AC", "#MC", "#XM", "#VE", "#CP", "---", "---",
    "---", "---", "---", "---", "#HV", "#VC", "#SX", "---"};

/* ------------------------------------------------------------ PIC 8259 */
/* = cavOS remap_pic() + disable_pic(): IRQ 0-7 -> 0x20, 8-15 -> 0x28, rồi che hết. */
static void remap_pic(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);   /* ICW1: init, có ICW4 */
    outb(0x21, 0x20); outb(0xA1, 0x28);   /* ICW2: vector base */
    outb(0x21, 0x04); outb(0xA1, 0x02);   /* ICW3: slave ở IRQ2 */
    outb(0x21, 0x01); outb(0xA1, 0x01);   /* ICW4: chế độ 8086 */
    outb(0x21, 0x00); outb(0xA1, 0x00);
    outb(0x21, 0xFF); outb(0xA1, 0xFF);   /* disable_pic(): che mọi IRQ (dùng APIC) */
}

void pic_dump(void) {
    serial_puts("[pic] remapped IRQ0-7 -> 0x20, IRQ8-15 -> 0x28; mask master=");
    serial_puthex_short(inb(0x21));
    serial_puts(" slave=");
    serial_puthex_short(inb(0xA1));
    serial_puts(" (all masked, like cavOS)\n");
}

/* ------------------------------------------------------- IRQ handler list */
/* cavOS: linked list dsIrqHandler (id, handler). Lab: mảng cố định 16 chỗ. */
static struct { uint8_t id; FunctionPtr handler; } irq_handlers[16];
static int irq_count;

void register_irq_handler(uint8_t vector, FunctionPtr handler) {
    if (irq_count < 16) {
        irq_handlers[irq_count].id = vector;
        irq_handlers[irq_count].handler = handler;
        irq_count++;
    }
}

/* ------------------------------------------------- lỗi có chủ đích (demo) */
static uint64_t expect_mask;
static uint8_t  expect_skip;
static int      expect_hit;

void isr_expect(uint64_t vector_mask, uint8_t skip) {
    expect_mask = vector_mask;
    expect_skip = skip;
    expect_hit  = 0;
}
int isr_expect_hit(void) { return expect_hit; }

/* ------------------------------------------------------------- báo cáo */
static uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

static void report(AsmPassedInterrupt *r) {
    uint64_t v = r->interrupt;
    serial_puts("[isr] vector ");
    serial_puthex_short(v);
    serial_putc(' ');
    serial_puts(v < 32 ? mnemonic[v] : "IRQ");
    serial_putc(' ');
    serial_puts(v < 32 ? exceptions[v] : "");
    serial_puts("  err=");
    serial_puthex_short(r->error);
    serial_puts("\n      RIP=");
    serial_puthex(r->rip);
    serial_puts(" CS=");
    serial_puthex_short(r->cs);
    serial_puts(" RFLAGS=");
    serial_puthex_short(r->rflags);
    serial_puts(" RSP=");
    serial_puthex(r->usermode_rsp);
    serial_puts(" SS=");
    serial_puthex_short(r->usermode_ss);
    serial_putc('\n');

    uint64_t here;
    __asm__ volatile("mov %%rsp, %0" : "=r"(here));
    serial_puts("      handler running on RSP=");
    serial_puthex(here);
    serial_putc('\n');

    if (v == 14) {
        uint64_t e = r->error;
        serial_puts("      CR2=");
        serial_puthex(read_cr2());
        serial_puts("  err bits: P=");
        serial_putdec(e & 1);
        serial_puts(" W=");
        serial_putdec((e >> 1) & 1);
        serial_puts(" U=");
        serial_putdec((e >> 2) & 1);
        serial_puts(" RSVD=");
        serial_putdec((e >> 3) & 1);
        serial_puts(" I=");
        serial_putdec((e >> 4) & 1);
        serial_puts((e & 1) ? "  (protection violation)\n" : "  (page not present)\n");
    }
    if (v == 10 || v == 11 || v == 12 || v == 13) {
        uint64_t e = r->error;
        serial_puts("      selector error code: EXT=");
        serial_putdec(e & 1);
        serial_puts(" IDT=");
        serial_putdec((e >> 1) & 1);
        serial_puts(" TI=");
        serial_putdec((e >> 2) & 1);
        serial_puts((e >> 1) & 1 ? " -> IDT vector " : " -> index ");
        serial_puthex_short((e >> 1) & 1 ? (e >> 3) & 0xff : (e >> 3));
        serial_putc('\n');
    }
}

static void register_dump(AsmPassedInterrupt *r) {
    serial_puts("      RAX="); serial_puthex(r->rax);
    serial_puts(" RBX="); serial_puthex(r->rbx);
    serial_puts(" RCX="); serial_puthex(r->rcx);
    serial_puts(" RDX="); serial_puthex(r->rdx);
    serial_puts("\n      RSI="); serial_puthex(r->rsi);
    serial_puts(" RDI="); serial_puthex(r->rdi);
    serial_puts(" RBP="); serial_puthex(r->rbp);
    serial_puts(" DS="); serial_puthex_short(r->ds);
    serial_putc('\n');
}

static void panic(void) {
    serial_puts("[kernel] Kernel panic triggered! (cli; hlt)\n");
    for (;;)
        __asm__ volatile("cli; hlt");
}

volatile uint64_t spurious_count;

/* = cavOS handle_interrupt(uint64_t rsp): rdi = RSP lúc isr_common gọi. */
void handle_interrupt(uint64_t rsp) {
    AsmPassedInterrupt *cpu = (AsmPassedInterrupt *)rsp;

    if (cpu->interrupt >= 32 && cpu->interrupt <= 47) {           /* IRQ */
        if (cpu->interrupt >= 40)
            outb(0xA0, 0x20);                                      /* EOI slave */
        outb(0x20, 0x20);                                          /* EOI master */
#ifdef EOI_LATE
        /* Lab 0x08 DEMO: với IRQ PCI (level-triggered, vector >= 0x24) gửi EOI SAU handler.
         * Mặc định (y cavOS) EOI trước handler: lúc đó card vẫn đang kéo INTx (ICR chưa đọc),
         * I/O APIC thấy mức vẫn cao nên gửi lại ngắt lần nữa -> handler chạy thêm 1 lần, ICR = 0.
         * (Không áp cho timer: schedule() không quay lại đây.) */
        if (cpu->interrupt >= 0x24) {
            for (int i = 0; i < irq_count; i++)
                if (irq_handlers[i].id == cpu->interrupt)
                    irq_handlers[i].handler(cpu);
            apic_eoi();
            return;
        }
#endif
        apic_eoi();                                                /* EOI Local APIC (0xB0) */
        for (int i = 0; i < irq_count; i++)
            if (irq_handlers[i].id == cpu->interrupt)
                irq_handlers[i].handler(cpu);
        return;
    }

    if (cpu->interrupt <= 31) {                                    /* exception */
        /* = cavOS: "To drop the current execution and give control to the scheduler, set
         * this variable and generate a page fault onto the magic address". */
        if (cpu->interrupt == 14 && tasksInitiated && currentTask->schedPageFault &&
            read_cr2() == SCHED_PAGE_FAULT_MAGIC_ADDRESS) {
            currentTask->schedPageFault = 0;
            cpu->rip += 2;              /* bỏ qua "movb (%rax),%al" (8A 00). cavOS: rip++ */
            schedule((uint64_t)cpu);
            return;
        }
        report(cpu);
        if (cpu->interrupt == 3) {                                 /* #BP là trap */
            serial_puts("      -> trap: RIP already points past int3, just iretq\n");
            expect_hit = 1;
            return;
        }
        if (cpu->interrupt < 64 && (expect_mask >> cpu->interrupt) & 1) {
            serial_puts("      -> expected fault: RIP += ");
            serial_putdec(expect_skip);
            serial_puts(" (skip the faulting instruction), iretq\n");
            cpu->rip += expect_skip;
            expect_mask = 0;
            expect_hit = 1;
            return;
        }
        register_dump(cpu);
        serial_puts("[isr] Kernel panic: ");
        serial_puts(exceptions[cpu->interrupt]);
        serial_puts("!\n");
        panic();
    }

    if (cpu->interrupt == 0x80) {
        serial_puts("[isr] int 0x80 (syscall gate, DPL 3) rax=");
        serial_puthex_short(cpu->rax);
        serial_putc('\n');
        return;
    }
    /* 0xff: APIC spurious -> không làm gì, không EOI (như cavOS) */
    if (cpu->interrupt == 0xff)
        spurious_count++;
}

/* = cavOS initiateISR(): remap PIC, 50 cổng, lidt, initiateAPIC(), sti. */
#ifndef NO_IST
static uint8_t df_stack[16384] __attribute__((aligned(16)));   /* stack riêng cho #DF */
#endif

void isr_init(void) {
    remap_pic();

    for (int i = 0; i < 48; i++)
        set_idt_gate(i, (uint64_t)asm_isr_redirect_table[i], 0x8E, 0);

    set_idt_gate(3, (uint64_t)asm_isr_redirect_table[3], 0xEE, 0);   /* user được int3 */
    set_idt_gate(0xff, (uint64_t)isr255, 0x8E, 0);                    /* APIC spurious */
    set_idt_gate(0x80, (uint64_t)isr128, 0xEE, 0);                    /* syscall DPL 3 */

#ifndef NO_IST
    /* Cải tiến của lab, cavOS KHÔNG làm: #DF chạy trên stack riêng IST1. */
    tss_set_ist1((uint64_t)(df_stack + sizeof(df_stack)));
    set_idt_gate(8, (uint64_t)asm_isr_redirect_table[8], 0x8E, 1);
#endif

    set_idt();
    apic_init();                  /* = initiateAPIC() (Bài 10) */
    __asm__ volatile("sti");      /* từ đây CPU nhận IRQ */
}
