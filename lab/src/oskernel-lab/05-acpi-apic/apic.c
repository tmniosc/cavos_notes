/* apic.c — Local APIC + I/O APIC, theo cavOS cpu/apic.c (Bài 10).
 *
 * Giống cavOS: apicCheck (CPUID.1:EDX bit 9), apicPhys từ MSR 0x1B rồi so với MADT,
 * duyệt MADT lấy I/O APIC (số chân đọc từ thanh ghi version), LAPIC address override,
 * apicSetBase, SVR |= 0x1FF; ioApicRedirect với Interrupt Source Override; vector cấp
 * bởi irqPerCoreAllocate = 32 + chỉ số.
 * Khác cavOS (ghi trên trang lab):
 *  1. MMIO map vào VA riêng với PCD|PWT (uncached). cavOS dùng thẳng HHDM (hhdmOffset + phys).
 *  2. Vòng duyệt MADT dừng ở madt + hdr.length (cavOS: start + hdr.length, quá 44 byte).
 *  3. irqPerCoreAllocate: so sánh "min = irqPerCpu[i]" (cavOS gán ngược "irqPerCpu[i] = min")
 *     và bỏ qua chỉ số có vector 0xFF (cavOS so chỉ số irqLast == 0xff).
 */
#include "apic.h"
#include "acpi.h"
#include "boot.h"
#include "paging.h"
#include "serial.h"

#define MMIO_VA 0xFFFFFE8000000000ULL   /* PML4 slot 509: trống, dành cho MMIO của lab */

typedef struct {                        /* = struct IOAPIC của cavOS (bỏ linked list) */
    uint8_t  id;
    uint64_t ioapicPhys;
    uint64_t ioapicVirt;
    int      ioapicRedStart;            /* GSI của chân 0 */
    int      ioapicRedEnd;              /* GSI của chân cuối (gồm cả nó), "NOT max!" */
} IOAPIC;

static IOAPIC   ioapics[4];
static int      ioapic_count;
static uint64_t apicPhys, apicVirt;
static uint64_t msr_before, msr_after;
static uint32_t svr_before, svr_after;
static uint32_t madt_lapic_addr;
static acpi_madt *madt;

/* ------------------------------------------------------------ CPU helpers */
static void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}
static uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static void wrmsr(uint32_t msr, uint64_t v) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}
static void halt_forever(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static int apicCheck(void) {
    uint32_t a, b, c, d;
    cpuid(1, &a, &b, &c, &d);
    return (d & (1 << 9)) != 0;             /* bit 9 EDX: có APIC on-chip */
}

/* ---------------------------------------------------------------- LAPIC */
void apicWrite(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(apicVirt + offset) = value;   /* luôn 32 bit, căn 16 byte */
}
uint32_t apicRead(uint32_t offset) {
    return *(volatile uint32_t *)(apicVirt + offset);
}
void apic_eoi(void) {
    apicWrite(APIC_REGISTER_EOI, 0);        /* ghi gì cũng được, cavOS ghi 0 */
}

static uint64_t apicGetBase(void) { return rdmsr(IA32_APIC_BASE_MSR) & 0xFFFFF000; }
static void apicSetBase(uint64_t apic) {
    wrmsr(IA32_APIC_BASE_MSR, apic | IA32_APIC_BASE_MSR_ENABLE | IA32_APIC_BASE_MSR_BSP);
}

/* --------------------------------------------------------------- I/O APIC */
/* Chỉ 2 thanh ghi MMIO: IOREGSEL (+0x00) chọn số thanh ghi, IOWIN (+0x10) đọc/ghi nó. */
static uint32_t ioApicRead(uint64_t virt, uint32_t reg) {
    volatile uint32_t *io = (volatile uint32_t *)virt;
    io[0] = reg & 0xff;
    return io[4];
}
static void ioApicWrite(uint64_t virt, uint32_t reg, uint32_t value) {
    volatile uint32_t *io = (volatile uint32_t *)virt;
    io[0] = reg & 0xff;
    io[4] = value;
}

/* Redirection entry 64 bit = 2 thanh ghi 0x10+2n (thấp) và 0x11+2n (cao). Y hệt cavOS. */
static void ioApicWriteRedEntry(uint64_t virt, uint8_t entry, uint8_t vector,
                                uint8_t delivery, uint8_t destmode, uint8_t polarity,
                                uint8_t mode, uint8_t mask, uint8_t dest) {
    uint32_t val = vector;
    val |= (uint32_t)(delivery & 0b111) << 8;   /* 000 = fixed */
    val |= (uint32_t)(destmode & 1) << 11;      /* 0 = physical (dest = APIC ID) */
    val |= (uint32_t)(polarity & 1) << 13;      /* 1 = active low */
    val |= (uint32_t)(mode & 1) << 15;          /* 1 = level */
    val |= (uint32_t)(mask & 1) << 16;          /* 1 = che */
    ioApicWrite(virt, 0x10 + entry * 2, val);
    ioApicWrite(virt, 0x11 + entry * 2, (uint32_t)dest << 24);
}

static IOAPIC *ioApicFetch(uint32_t gsi) {
    for (int i = 0; i < ioapic_count; i++)
        if ((int)gsi >= ioapics[i].ioapicRedStart && (int)gsi <= ioapics[i].ioapicRedEnd)
            return &ioapics[i];
    return 0;
}

/* --------------------------------------------------- cấp vector (như cavOS) */
static uint8_t  irqPerCpu[64];             /* cavOS: calloc(cpu_count) */
static uint8_t  irqGenericArray[MAX_IRQ];  /* chỉ số -> GSI */
static uint32_t lapicGenericArray[MAX_IRQ];
static int      irqLast;

uint8_t irqPerCoreAllocate(uint8_t gsi, uint32_t *lapicId) {
    for (int i = 0; i < irqLast; i++)           /* GSI đã có vector -> dùng lại */
        if (irqGenericArray[i] == gsi) {
            *lapicId = lapicGenericArray[i];
            return 32 + i;
        }

    /* cavOS: while (irqLast == 0xff) — so CHỈ SỐ với 0xff. Vector spurious là
     * 32 + chỉ số == 0xff, tức chỉ số 0xdf. Lab so đúng vector. */
    while (irqLast + 32 == 0xff)
        irqLast++;
    if (irqLast + 32 > 0xff) {
        serial_puts("[irq] vector overflow!\n");
        halt_forever();
    }

    /* Chọn CPU ít IRQ nhất. cavOS: for (i < 1) // todo: bootloader.smp->cpu_count
     * Lab cũng chỉ xét CPU 0: các AP vẫn đang đứng chờ trong Limine (chưa bật ngắt),
     * gửi IRQ sang đó thì không ai xử lý. cavOS ghi "irqPerCpu[i] = min" (gán),
     * lab ghi đúng ý: "min = irqPerCpu[i]". */
    int min = MAX_IRQ;
    int minIndex = 0;
    for (int i = 0; i < 1; i++) {
        if (irqPerCpu[i] < min) {
            min = irqPerCpu[i];
            minIndex = i;
        }
    }

    struct limine_smp_response *smp = boot_smp();
    uint32_t lapic = smp ? smp->cpus[minIndex]->lapic_id : 0;
    irqGenericArray[irqLast] = gsi;
    lapicGenericArray[irqLast] = lapic;
    irqPerCpu[minIndex]++;
    *lapicId = lapic;
    return (uint8_t)(32 + irqLast++);
}

/* = cavOS ioApicRedirect: tra Interrupt Source Override trong MADT trước. */
uint8_t ioApicRedirect(uint8_t irq, int masked) {
    uint8_t  polarity = 0;          /* ISA mặc định: active high */
    uint8_t  trigger = 0;           /* edge */
    uint32_t gsi = irq;

    uint64_t cur = (uint64_t)madt + sizeof(acpi_madt);
    uint64_t end = (uint64_t)madt + madt->hdr.length;
    while (cur < end) {
        acpi_entry_hdr *e = (acpi_entry_hdr *)cur;
        if (e->type == MADT_ISO && ((acpi_madt_iso *)e)->source == irq) {
            acpi_madt_iso *o = (acpi_madt_iso *)e;
            gsi = o->gsi;
            polarity = (o->flags & 2) ? 1 : 0;   /* 11 = active low */
            trigger = (o->flags & 8) ? 1 : 0;    /* 11 = level */
            break;
        }
        cur += e->length;
    }

    IOAPIC *io = ioApicFetch(gsi);
    if (!io) {
        serial_puts("[apic] FATAL! no I/O APIC for this GSI\n");
        halt_forever();
    }

    uint32_t lapic = 0;
    uint8_t  vector = irqPerCoreAllocate((uint8_t)gsi, &lapic);
    uint8_t  entry = (uint8_t)(gsi - io->ioapicRedStart);
    ioApicWriteRedEntry(io->ioapicVirt, entry, vector, 0, 0, polarity, trigger,
                        masked ? 1 : 0, (uint8_t)lapic);

    serial_puts("[ioapic] ISA IRQ ");
    serial_putdec(irq);
    serial_puts(gsi != irq ? " -> override -> GSI " : " -> (no override) GSI ");
    serial_putdec(gsi);
    serial_puts(" = I/O APIC pin ");
    serial_putdec(entry);
    serial_puts(" -> vector ");
    serial_puthex_short(vector);
    serial_puts(" -> LAPIC ");
    serial_putdec(lapic);
    serial_puts(polarity ? " active-low" : " active-high");
    serial_puts(trigger ? " level" : " edge");
    serial_puts(masked ? " MASKED\n" : "\n");
    return vector;
}

/* ------------------------------------------------------------------ init */
static uint64_t map_uncached(uint64_t phys, int slot) {
    uint64_t va = MMIO_VA + (uint64_t)slot * 0x1000;
    vmap(va, phys & ~0xFFFULL, PTE_RW | PTE_PCD | PTE_PWT, 0);
    return va + (phys & 0xFFF);
}

/* = cavOS initiateAPIC() */
void apic_init(void) {
    if (!apicCheck()) {
        serial_puts("[apic] FATAL! APIC is not supported!\n");
        halt_forever();
    }

    msr_before = rdmsr(IA32_APIC_BASE_MSR);
    apicPhys = apicGetBase();

    madt = acpi_madt_get();                      /* cavOS: uacpi_table_find_by_signature("APIC") */
    if (!madt) {
        serial_puts("[apic] Couldn't find MADT table\n");
        halt_forever();
    }
    madt_lapic_addr = madt->local_interrupt_controller_address;
    if (madt_lapic_addr != apicPhys) {
        serial_puts("[apic] Warning! MADT physical address doesn't match MSR\n");
        apicPhys = madt_lapic_addr;
    }

    uint64_t cur = (uint64_t)madt + sizeof(acpi_madt);
    uint64_t end = (uint64_t)madt + madt->hdr.length;   /* cavOS: + sizeof(acpi_madt) thừa */
    while (cur < end) {
        acpi_entry_hdr *e = (acpi_entry_hdr *)cur;
        if (e->type == MADT_IOAPIC && ioapic_count < 4) {
            acpi_madt_ioapic *s = (acpi_madt_ioapic *)e;
            IOAPIC *io = &ioapics[ioapic_count];
            io->id = s->id;
            io->ioapicPhys = s->address;
            io->ioapicVirt = map_uncached(s->address, 1 + ioapic_count);
            io->ioapicRedStart = (int)s->gsi_base;
            int capacity = (ioApicRead(io->ioapicVirt, 1) >> 16) & 0xFF;   /* max redir index */
            io->ioapicRedEnd = io->ioapicRedStart + capacity;
            ioapic_count++;
        } else if (e->type == MADT_LAPIC_ADDRESS_OVERRIDE) {
            apicPhys = ((acpi_madt_lapic_override *)e)->address;
        }
        cur += e->length;
    }
    if (!ioapic_count) {
        serial_puts("[apic] No I/O APICs found!\n");
        halt_forever();
    }

    apicVirt = map_uncached(apicPhys, 0);        /* cavOS: hhdmOffset + apicPhys */

    serial_puts("[apic] Detection completed: lapic{");
    serial_puthex_short(apicPhys);
    serial_puts("} ");
    for (int i = 0; i < ioapic_count; i++) {
        serial_puts("ioapic{");
        serial_puthex_short(ioapics[i].ioapicPhys);
        serial_puts("} ");
    }
    serial_putc('\n');

    /* bật LAPIC cho BSP */
    apicSetBase(apicPhys);
    msr_after = rdmsr(IA32_APIC_BASE_MSR);
    svr_before = apicRead(APIC_REGISTER_SPURIOUS);
    apicWrite(APIC_REGISTER_SPURIOUS, svr_before | 0x1FF);   /* bit 8 = enable, vector 0xFF */
    svr_after = apicRead(APIC_REGISTER_SPURIOUS);
}

/* ------------------------------------------------------------------ dump */
static void flags_of(const char *what, uint64_t va) {
    int level = 0;
    uint64_t e = paging_leaf(va, &level);
    serial_puts(what);
    serial_puthex(va);
    if (!e) {
        serial_puts(" -> not mapped\n");
        return;
    }
    serial_puts(level == 1 ? " -> 4K page" : level == 2 ? " -> 2M page" : " -> 1G page");
    serial_puts(" entry=");
    serial_puthex(e);
    serial_puts(" PWT=");
    serial_putdec((e >> 3) & 1);
    serial_puts(" PCD=");
    serial_putdec((e >> 4) & 1);
    serial_puts(((e >> 3) & 3) == 3 ? " (UC)\n" : ((e >> 3) & 3) == 0 ? " (WB, cached)\n" : "\n");
}

void ioapic_dump_entries(int first, int count) {
    IOAPIC *io = &ioapics[0];
    for (int g = first; g < first + count; g++) {
        uint32_t lo = ioApicRead(io->ioapicVirt, 0x10 + g * 2);
        uint32_t hi = ioApicRead(io->ioapicVirt, 0x11 + g * 2);
        serial_puts("[ioapic]   pin ");
        serial_putdec_right(g, 2);
        serial_puts(": ");
        serial_puthex(((uint64_t)hi << 32) | lo);
        serial_puts("  vector=");
        serial_puthex_short(lo & 0xff);
        serial_puts(lo & (1 << 16) ? " masked" : " ACTIVE");
        serial_puts(lo & (1 << 15) ? " level" : " edge");
        serial_puts(lo & (1 << 13) ? " low" : " high");
        serial_puts(" dest=");
        serial_putdec(hi >> 24);
        serial_putc('\n');
    }
}

void apic_dump(void) {
    uint32_t a, b, c, d;
    cpuid(1, &a, &b, &c, &d);
    serial_puts("[apic] CPUID.1: EDX.APIC(bit9)=");
    serial_putdec((d >> 9) & 1);
    serial_puts(" ECX.x2APIC(bit21)=");
    serial_putdec((c >> 21) & 1);
    serial_puts("  (cavOS uses xAPIC MMIO only)\n");

    serial_puts("[apic] IA32_APIC_BASE (MSR 0x1B) before=");
    serial_puthex_short(msr_before);
    serial_puts(" after=");
    serial_puthex_short(msr_after);
    serial_puts("  (bits 31:12 base, bit 8 BSP, bit 11 enable)\n");
    serial_puts("[apic] MADT says ");
    serial_puthex_short(madt_lapic_addr);
    serial_puts(", MSR says ");
    serial_puthex_short(msr_before & 0xFFFFF000);
    serial_puts(madt_lapic_addr == (msr_before & 0xFFFFF000) ? " -> match\n" : " -> MISMATCH\n");

    flags_of("[apic] HHDM view of LAPIC ", hhdmOffset + apicPhys);
    flags_of("[apic] lab MMIO map        ", apicVirt & ~0xFFFULL);

    /* MTRR: loại bộ nhớ theo dải PA do firmware đặt. MTRR = UC thắng PAT = WB
     * (Intel SDM Vol.3 "Selecting Memory Types for Pentium III and More Recent ... Processors"). */
    uint64_t cap = rdmsr(0xFE), def = rdmsr(0x2FF);
    serial_puts("[mtrr] MTRRdefType=");
    serial_puthex_short(def);
    serial_puts(" (default type ");
    serial_putdec(def & 0xff);
    serial_puts(def & (1 << 11) ? ", enabled)" : ", MTRRs disabled)");
    serial_puts("  variable ranges=");
    serial_putdec(cap & 0xff);
    serial_putc('\n');
    for (uint32_t i = 0; i < (cap & 0xff); i++) {
        uint64_t base = rdmsr(0x200 + 2 * i), mask = rdmsr(0x201 + 2 * i);
        if (!(mask & (1 << 11)))
            continue;
        uint64_t pa_mask = mask & 0x000FFFFFFFFFF000ULL;
        int hit = ((apicPhys & pa_mask) == (base & pa_mask));
        serial_puts("[mtrr]   var ");
        serial_putdec(i);
        serial_puts(": base=");
        serial_puthex_short(base & 0x000FFFFFFFFFF000ULL);
        serial_puts(" mask=");
        serial_puthex_short(pa_mask);
        serial_puts(" type=");
        serial_putdec(base & 0xff);
        serial_puts((base & 0xff) == 0 ? " (UC)" : (base & 0xff) == 6 ? " (WB)" : "");
        serial_puts(hit ? "  <- covers the LAPIC page\n" : "\n");
    }

    uint32_t ver = apicRead(APIC_REGISTER_VERSION);
    serial_puts("[lapic] ID reg=");
    serial_puthex_short(apicRead(APIC_REGISTER_ID));
    serial_puts(" -> APIC ID ");
    serial_putdec(apicRead(APIC_REGISTER_ID) >> 24);
    serial_puts("  version reg=");
    serial_puthex_short(ver);
    serial_puts(" -> version ");
    serial_puthex_short(ver & 0xff);
    serial_puts(", ");
    serial_putdec(((ver >> 16) & 0xff) + 1);
    serial_puts(" LVT entries\n");
    serial_puts("[lapic] SVR (0xF0) before=");
    serial_puthex_short(svr_before);
    serial_puts(" after |= 0x1FF -> ");
    serial_puthex_short(svr_after);
    serial_puts(" (bit 8 APIC enabled, spurious vector ");
    serial_puthex_short(svr_after & 0xff);
    serial_puts(")\n");

    for (int i = 0; i < ioapic_count; i++) {
        IOAPIC *io = &ioapics[i];
        uint32_t v = ioApicRead(io->ioapicVirt, 1);
        serial_puts("[ioapic] id=");
        serial_putdec((ioApicRead(io->ioapicVirt, 0) >> 24) & 0xf);
        serial_puts(" PA=");
        serial_puthex_short(io->ioapicPhys);
        serial_puts(" version reg=");
        serial_puthex_short(v);
        serial_puts(" -> version ");
        serial_puthex_short(v & 0xff);
        serial_puts(", max redirection index ");
        serial_putdec((v >> 16) & 0xff);
        serial_puts(" -> ");
        serial_putdec(((v >> 16) & 0xff) + 1);
        serial_puts(" pins, GSI ");
        serial_putdec(io->ioapicRedStart);
        serial_puts("..");
        serial_putdec(io->ioapicRedEnd);
        serial_putc('\n');
    }

    struct limine_smp_response *smp = boot_smp();
    if (smp) {
        serial_puts("[smp] Limine: cpu_count=");
        serial_putdec(smp->cpu_count);
        serial_puts(" bsp_lapic_id=");
        serial_putdec(smp->bsp_lapic_id);
        serial_puts(" lapic ids:");
        for (uint64_t i = 0; i < smp->cpu_count; i++) {
            serial_putc(' ');
            serial_putdec(smp->cpus[i]->lapic_id);
        }
        serial_putc('\n');
    }
}
