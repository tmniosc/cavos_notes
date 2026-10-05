/* acpi.c — RSDP -> RSDT/XSDT -> bảng, đọc bằng tay (Bài 8).
 *
 * cavOS dùng uACPI cho mọi việc này (acpi/acpi.c: initiateACPI). Lab làm lại phần
 * "tìm bảng" giống uACPI tables.c: revision > 1 và có xsdt_addr thì dùng XSDT (con trỏ
 * 8 byte), không thì RSDT (con trỏ 4 byte). Mọi PA đọc qua HHDM, như
 * uacpi_kernel_map() của cavOS (return hhdmOffset + addr).
 * Phần uACPI làm mà lab KHÔNG làm: chạy AML trong DSDT/SSDT (namespace, _STA/_INI...).
 */
#include "acpi.h"
#include "boot.h"
#include "serial.h"

#define MAX_TABLES 32

static acpi_rsdp    *rsdp;
static acpi_sdt_hdr *rxsdt;               /* RSDT hoặc XSDT */
static int           rxsdt_entry_size;    /* 4 hoặc 8 */
static uint64_t      table_pa[MAX_TABLES];
static acpi_sdt_hdr *table[MAX_TABLES];
static int           table_count;

/* ----------------------------------------------------------- tiện ích */
static uint8_t sum8(const void *p, uint64_t n) {
    const uint8_t *b = (const uint8_t *)p;
    uint8_t s = 0;
    for (uint64_t i = 0; i < n; i++)
        s += b[i];
    return s;
}

static int sig_eq(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static void putn(const char *s, int n) {
    for (int i = 0; i < n; i++)
        serial_putc(s[i] >= 0x20 && s[i] < 0x7f ? s[i] : '.');
}

static uint32_t rd32(const void *base, int off) {
    const uint8_t *b = (const uint8_t *)base + off;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static uint16_t rd16(const void *base, int off) {
    const uint8_t *b = (const uint8_t *)base + off;
    return (uint16_t)(b[0] | (b[1] << 8));
}
static uint64_t rd64(const void *base, int off) {
    return (uint64_t)rd32(base, off) | ((uint64_t)rd32(base, off + 4) << 32);
}

/* Một dòng giống log uACPI: "APIC 0x... 00000078 v03 (BOCHS  BXPC    )" + checksum. */
static void table_line(const char *prefix, uint64_t pa, const acpi_sdt_hdr *h) {
    serial_puts(prefix);
    putn(h->signature, 4);
    serial_puts(" PA=");
    serial_puthex(pa);
    serial_puts(" len=");
    serial_puthex_short(h->length);
    serial_puts(" rev=");
    serial_putdec(h->revision);
    serial_puts(" OEM=(");
    putn(h->oemid, 6);
    serial_putc(' ');
    putn(h->oem_table_id, 8);
    serial_puts(") checksum ");
    serial_puts(sum8(h, h->length) == 0 ? "OK" : "BAD");
    serial_putc('\n');
}

/* --------------------------------------------------------------- init */
void acpi_init(void) {
    uint64_t pa = boot_rsdp_phys();
    if (pa == 0)
        return;
    rsdp = (acpi_rsdp *)P2V(pa);

    uint64_t rx_pa;
    if (rsdp->revision > 1 && rsdp->xsdt_addr) {     /* như uACPI tables.c */
        rx_pa = rsdp->xsdt_addr;
        rxsdt_entry_size = 8;
    } else {
        rx_pa = rsdp->rsdt_addr;
        rxsdt_entry_size = 4;
    }
    rxsdt = (acpi_sdt_hdr *)P2V(rx_pa);

    int n = (int)((rxsdt->length - sizeof(acpi_sdt_hdr)) / rxsdt_entry_size);
    const uint8_t *entries = (const uint8_t *)rxsdt + sizeof(acpi_sdt_hdr);
    for (int i = 0; i < n && table_count < MAX_TABLES; i++) {
        uint64_t tpa = rxsdt_entry_size == 8 ? rd64(entries, i * 8) : rd32(entries, i * 4);
        table_pa[table_count] = tpa;
        table[table_count] = (acpi_sdt_hdr *)P2V(tpa);
        table_count++;
    }
}

acpi_sdt_hdr *acpi_find(const char *sig) {
    for (int i = 0; i < table_count; i++)
        if (sig_eq(table[i]->signature, sig, 4))
            return table[i];
    return 0;
}

acpi_madt *acpi_madt_get(void) {
    return (acpi_madt *)acpi_find("APIC");
}

/* ----------------------------------------------------------------- dump */
static void dump_fadt(void) {
    acpi_sdt_hdr *f = acpi_find("FACP");
    if (!f)
        return;
    /* offset theo ACPI §5.2.9 (bảng FADT) */
    uint64_t dsdt = rd32(f, 40), facs = rd32(f, 36);
    if (f->length >= 148 && rd64(f, 140))
        dsdt = rd64(f, 140);                         /* X_DSDT (ACPI 2.0+) */
    if (f->length >= 140 && rd64(f, 132))
        facs = rd64(f, 132);                         /* X_FIRMWARE_CTRL */

    if (dsdt) {
        acpi_sdt_hdr *d = (acpi_sdt_hdr *)P2V(dsdt);
        table_line("[acpi]   (via FADT) ", dsdt, d);
        serial_puts("[acpi]        DSDT body = ");
        serial_putdec(d->length - sizeof(acpi_sdt_hdr));
        serial_puts(" bytes of AML, first opcode ");
        serial_puthex_short(((uint8_t *)d)[sizeof(acpi_sdt_hdr)]);
        serial_puts(" (0x10 = ScopeOp) -> needs an AML interpreter (uACPI in cavOS)\n");
    }
    if (facs) {
        const char *s = (const char *)P2V(facs);
        serial_puts("[acpi]   (via FADT) ");
        putn(s, 4);
        serial_puts(" PA=");
        serial_puthex(facs);
        serial_puts(" len=");
        serial_puthex_short(rd32(s, 4));
        serial_puts(" (no checksum field)\n");
    }

    serial_puts("[fadt] SCI_INT=");
    serial_putdec(rd16(f, 46));
    serial_puts(" SMI_CMD=");
    serial_puthex_short(rd32(f, 48));
    serial_puts(" PM1a_CNT=");
    serial_puthex_short(rd32(f, 64));
    serial_puts(" PM_TMR=");
    serial_puthex_short(rd32(f, 76));
    serial_puts(" century=");
    serial_puthex_short(((uint8_t *)f)[108]);
    serial_puts(" IAPC_BOOT_ARCH=");
    serial_puthex_short(rd16(f, 109));
    serial_putc('\n');
}

static const char *polarity(uint16_t f) {
    switch (f & 3) {
    case 0: return "bus-default";
    case 1: return "active-high";
    case 3: return "active-low";
    default: return "reserved";
    }
}
static const char *trigger(uint16_t f) {
    switch ((f >> 2) & 3) {
    case 0: return "bus-default";
    case 1: return "edge";
    case 3: return "level";
    default: return "reserved";
    }
}

static void dump_entry(const acpi_entry_hdr *e) {
    serial_puts("[madt]   type ");
    serial_putdec(e->type);
    serial_puts(" len ");
    serial_putdec_right(e->length, 2);
    serial_puts("  ");
    switch (e->type) {
    case MADT_LAPIC: {
        const acpi_madt_lapic *l = (const acpi_madt_lapic *)e;
        serial_puts("Processor Local APIC  uid=");
        serial_putdec(l->uid);
        serial_puts(" apic_id=");
        serial_putdec(l->id);
        serial_puts(" flags=");
        serial_puthex_short(l->flags);
        serial_puts(l->flags & 1 ? " (enabled)" : " (disabled)");
        break;
    }
    case MADT_IOAPIC: {
        const acpi_madt_ioapic *io = (const acpi_madt_ioapic *)e;
        serial_puts("I/O APIC  id=");
        serial_putdec(io->id);
        serial_puts(" address=");
        serial_puthex_short(io->address);
        serial_puts(" gsi_base=");
        serial_putdec(io->gsi_base);
        break;
    }
    case MADT_ISO: {
        const acpi_madt_iso *o = (const acpi_madt_iso *)e;
        serial_puts("Interrupt Source Override  bus=");
        serial_putdec(o->bus);
        serial_puts(" IRQ ");
        serial_putdec(o->source);
        serial_puts(" -> GSI ");
        serial_putdec(o->gsi);
        serial_puts("  flags=");
        serial_puthex_short(o->flags);
        serial_puts(" (");
        serial_puts(polarity(o->flags));
        serial_puts(", ");
        serial_puts(trigger(o->flags));
        serial_putc(')');
        break;
    }
    case MADT_LAPIC_NMI: {
        const acpi_madt_lapic_nmi *n = (const acpi_madt_lapic_nmi *)e;
        serial_puts("Local APIC NMI  uid=");
        serial_puthex_short(n->uid);
        serial_puts(n->uid == 0xff ? " (all CPUs)" : "");
        serial_puts(" LINT");
        serial_putdec(n->lint);
        serial_puts(" flags=");
        serial_puthex_short(n->flags);
        break;
    }
    case MADT_LAPIC_ADDRESS_OVERRIDE: {
        const acpi_madt_lapic_override *o = (const acpi_madt_lapic_override *)e;
        serial_puts("Local APIC Address Override  address=");
        serial_puthex(o->address);
        break;
    }
    default:
        serial_puts("(not decoded by the lab)");
    }
    serial_putc('\n');
}

static void dump_madt(void) {
    acpi_madt *m = acpi_madt_get();
    if (!m) {
        serial_puts("[madt] no APIC table!\n");
        return;
    }
    serial_puts("[madt] local APIC address = ");
    serial_puthex_short(m->local_interrupt_controller_address);
    serial_puts("  flags = ");
    serial_puthex_short(m->flags);
    serial_puts(m->flags & 1 ? " (PCAT_COMPAT: legacy 8259 PICs present)\n" : "\n");

    uint64_t cur = (uint64_t)m + sizeof(acpi_madt);
    uint64_t end = (uint64_t)m + m->hdr.length;            /* đúng: hết bảng */
    int counts[16] = {0};
    while (cur < end) {
        const acpi_entry_hdr *e = (const acpi_entry_hdr *)cur;
        if (e->length < 2)
            break;
        dump_entry(e);
        if (e->type < 16)
            counts[e->type]++;
        cur += e->length;
    }
    serial_puts("[madt] totals: ");
    serial_putdec(counts[MADT_LAPIC]);
    serial_puts(" local APIC, ");
    serial_putdec(counts[MADT_IOAPIC]);
    serial_puts(" I/O APIC, ");
    serial_putdec(counts[MADT_ISO]);
    serial_puts(" overrides, ");
    serial_putdec(counts[MADT_LAPIC_NMI]);
    serial_puts(" LAPIC NMI\n");

    /* cavOS (initiateAPIC, ioApicRedirect): curr = madt + sizeof(acpi_madt);
     * end = curr + madt->hdr.length  -> quá cuối bảng sizeof(acpi_madt) = 44 byte.
     * Xem vòng lặp đó sẽ đọc thêm gì sau cuối bảng thật (chỉ đọc, không dùng). */
    uint64_t cav_end = (uint64_t)m + sizeof(acpi_madt) + m->hdr.length;
    serial_puts("[madt] cavOS loop end = start + hdr.length -> walks ");
    serial_putdec(cav_end - end);
    serial_puts(" bytes past the table; bytes there:");
    const uint8_t *b = (const uint8_t *)end;
    for (uint64_t i = 0; i < cav_end - end; i++) {
        if (i % 22 == 0)
            serial_puts("\n[madt]   ");
        serial_putc("0123456789abcdef"[b[i] >> 4]);
        serial_putc("0123456789abcdef"[b[i] & 15]);
        serial_putc(' ');
    }
    serial_putc('\n');
    cur = end;
    while (cur < cav_end) {
        const acpi_entry_hdr *e = (const acpi_entry_hdr *)cur;
        serial_puts("[madt]   cavOS would read a fake entry: type ");
        serial_putdec(e->type);
        serial_puts(" len ");
        serial_putdec(e->length);
        if (e->length == 0) {
            serial_puts(" -> curr += 0: infinite loop\n");
            break;
        }
        serial_putc('\n');
        cur += e->length;
    }
}

void acpi_dump(void) {
    serial_puts("[acpi] Limine RSDP response->address = ");
    serial_puthex(boot_rsdp_raw());
    serial_puts(" (HHDM VA, base revision 2)\n");
    if (!rsdp) {
        serial_puts("[acpi] no RSDP!\n");
        return;
    }
    serial_puts("[acpi] RSDP PA=");
    serial_puthex(boot_rsdp_phys());
    serial_puts(" sig=\"");
    putn(rsdp->signature, 8);
    serial_puts("\" rev=");
    serial_putdec(rsdp->revision);
    serial_puts(" OEM=(");
    putn(rsdp->oemid, 6);
    serial_puts(") checksum(20B) ");
    serial_puts(sum8(rsdp, 20) == 0 ? "OK" : "BAD");
    if (rsdp->revision >= 2) {
        serial_puts(" ext-checksum(36B) ");
        serial_puts(sum8(rsdp, rsdp->length) == 0 ? "OK" : "BAD");
    }
    serial_puts("\n[acpi] RSDT=");
    serial_puthex_short(rsdp->rsdt_addr);
    serial_puts(" XSDT=");
    serial_puthex_short(rsdp->revision >= 2 ? rsdp->xsdt_addr : 0);
    serial_puts(rxsdt_entry_size == 8 ? " -> using XSDT (8-byte pointers)\n"
                                      : " -> using RSDT (4-byte pointers)\n");

    table_line("[acpi] ", (uint64_t)rxsdt - hhdmOffset, rxsdt);
    serial_puts("[acpi] ");
    serial_putdec(table_count);
    serial_puts(" tables listed:\n");
    for (int i = 0; i < table_count; i++)
        table_line("[acpi]   ", table_pa[i], table[i]);
    dump_fadt();
    dump_madt();
}
