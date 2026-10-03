/* apic.h — Local APIC + I/O APIC, theo cavOS cpu/apic.c + include/apic.h (Bài 10). */
#pragma once
#include <stdint.h>

#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_BSP    0x100     /* bit 8: CPU này là BSP */
#define IA32_APIC_BASE_MSR_ENABLE 0x800     /* bit 11: bật xAPIC */

/* offset thanh ghi Local APIC (Intel SDM Vol.3 bảng "Local APIC Register Address Map") */
#define APIC_REGISTER_ID            0x020
#define APIC_REGISTER_VERSION       0x030
#define APIC_REGISTER_EOI           0x0B0
#define APIC_REGISTER_SPURIOUS      0x0F0
#define APIC_REGISTER_LVT_TIMER     0x320
#define APIC_REGISTER_TIMER_INITCNT 0x380
#define APIC_REGISTER_TIMER_CURRCNT 0x390
#define APIC_REGISTER_TIMER_DIV     0x3E0

#define APIC_LVT_TIMER_MODE_PERIODIC (1 << 17)
#define APIC_LVT_MASKED              (1 << 16)

#define MAX_IRQ 256

void     apic_init(void);                  /* = cavOS initiateAPIC() */
void     apic_dump(void);

uint32_t apicRead(uint32_t offset);
void     apicWrite(uint32_t offset, uint32_t value);
void     apic_eoi(void);

/* = cavOS ioApicRedirect(irq, ignored): IRQ ISA -> (override) -> GSI -> vector.
 * masked = 1 thì ghi entry nhưng che (cavOS dùng để tắt PIT cũ). */
uint8_t  ioApicRedirect(uint8_t irq, int masked);
uint8_t  irqPerCoreAllocate(uint8_t gsi, uint32_t *lapicId);
void     ioapic_dump_entries(int first, int count);
