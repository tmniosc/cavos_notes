/* gdt.h — GDT + TSS giống cavOS (src/kernel/include/gdt.h, cpu/gdt.c), Bài 7.
 * Cùng thứ tự descriptor, cùng selector: 0x28 kernel code, 0x30 kernel data,
 * 0x48 user data, 0x50 user code (data TRƯỚC code vì sysret), 0x58 TSS.
 */
#pragma once
#include <stdint.h>

typedef struct GDTEntry {          /* descriptor code/data, 8 byte */
    uint16_t limit;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;          /* nửa cao = cờ G, D/B, L; nửa thấp = limit 19:16 */
    uint8_t  base_high;
} __attribute__((packed)) GDTEntry;

typedef struct TSSEntry {          /* descriptor hệ thống TSS, 16 byte (base 64-bit) */
    uint16_t length;               /* thật ra là limit */
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;               /* P | DPL | type 1001 = available 64-bit TSS */
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper32;
    uint32_t reserved;
} __attribute__((packed)) TSSEntry;

typedef struct TSSPtr {            /* nội dung TSS 64-bit, 104 byte */
    uint32_t unused0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t unused1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t unused2;
    uint32_t iopb;                 /* 16 bit cao = I/O map base */
} __attribute__((packed)) TSSPtr;

typedef struct GDTEntries {
    GDTEntry descriptors[11];
    TSSEntry tss;
} __attribute__((packed)) GDTEntries;

typedef struct GDTPtr {            /* giá trị nạp vào GDTR */
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) GDTPtr;

#define GDT_KERNEL_CODE 0x28       /* 40 */
#define GDT_KERNEL_DATA 0x30       /* 48 */
#define GDT_USER_DATA   0x48       /* 72 */
#define GDT_USER_CODE   0x50       /* 80 */
#define GDT_TSS_SEL     0x58       /* 88 (cavOS ghi nhầm GDT_TSS = 80, xem Bài 7) */

extern TSSPtr *tssPtr;

void gdt_init(void);               /* = initiateGDT(): điền, lgdt, lretq, ltr */
void gdt_dump(void);               /* in GDTR/TR/selector + từng qword của bảng */
void tss_set_ist1(uint64_t stack_top);
