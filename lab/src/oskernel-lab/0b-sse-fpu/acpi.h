/* acpi.h — đọc bảng ACPI BẰNG TAY: RSDP -> RSDT/XSDT -> từng bảng, MADT, FADT (Bài 8).
 *
 * cavOS KHÔNG tự parse: initiateACPI() (acpi/acpi.c) gọi thư viện uACPI
 * (uacpi_initialize -> uacpi_namespace_load -> uacpi_namespace_initialize), rồi
 * initiateAPIC() lấy MADT bằng uacpi_table_find_by_signature("APIC").
 * Lab chỉ cần phần "bảng" (không chạy AML trong DSDT), nên tự đọc theo ACPI spec §5.2.
 * Tên struct/trường lấy theo include/uacpi/acpi.h của cavOS.
 */
#pragma once
#include <stdint.h>

typedef struct {                    /* RSDP (ACPI §5.2.5.3) */
    char     signature[8];          /* "RSD PTR " */
    uint8_t  checksum;              /* 20 byte đầu cộng lại = 0 */
    char     oemid[6];
    uint8_t  revision;              /* 0 = ACPI 1.0 (chỉ RSDT), 2 = ACPI 2.0+ (có XSDT) */
    uint32_t rsdt_addr;
    /* từ đây chỉ có khi revision >= 2 */
    uint32_t length;                /* 36 */
    uint64_t xsdt_addr;
    uint8_t  extended_checksum;     /* cả 36 byte cộng lại = 0 */
    uint8_t  rsvd[3];
} __attribute__((packed)) acpi_rsdp;

typedef struct {                    /* header chung 36 byte của mọi bảng (§5.2.6) */
    char     signature[4];
    uint32_t length;                /* cả bảng, tính cả header */
    uint8_t  revision;
    uint8_t  checksum;
    char     oemid[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_hdr;

typedef struct {                    /* MADT, chữ ký "APIC" (§5.2.12) */
    acpi_sdt_hdr hdr;
    uint32_t local_interrupt_controller_address;   /* PA local APIC (32-bit) */
    uint32_t flags;                                /* bit 0 = PCAT_COMPAT: có 8259 */
    /* theo sau: dãy entry {type, length, ...} tới hết hdr.length */
} __attribute__((packed)) acpi_madt;               /* 44 byte, như uACPI */

typedef struct { uint8_t type, length; } __attribute__((packed)) acpi_entry_hdr;

enum {
    MADT_LAPIC = 0, MADT_IOAPIC = 1, MADT_ISO = 2, MADT_NMI_SOURCE = 3,
    MADT_LAPIC_NMI = 4, MADT_LAPIC_ADDRESS_OVERRIDE = 5, MADT_X2APIC = 9,
};

typedef struct {                    /* type 0: Processor Local APIC, 8 byte */
    acpi_entry_hdr hdr;
    uint8_t  uid;                   /* ACPI processor UID */
    uint8_t  id;                    /* local APIC ID */
    uint32_t flags;                 /* bit 0 enabled, bit 1 online capable */
} __attribute__((packed)) acpi_madt_lapic;

typedef struct {                    /* type 1: I/O APIC, 12 byte */
    acpi_entry_hdr hdr;
    uint8_t  id;
    uint8_t  rsvd;
    uint32_t address;               /* PA thanh ghi IOREGSEL/IOWIN */
    uint32_t gsi_base;              /* GSI của chân 0 */
} __attribute__((packed)) acpi_madt_ioapic;

typedef struct {                    /* type 2: Interrupt Source Override, 10 byte */
    acpi_entry_hdr hdr;
    uint8_t  bus;                   /* 0 = ISA */
    uint8_t  source;                /* IRQ ISA */
    uint32_t gsi;                   /* chân thật trên I/O APIC */
    uint16_t flags;                 /* bit 1:0 polarity, 3:2 trigger (MPS INTI) */
} __attribute__((packed)) acpi_madt_iso;

typedef struct {                    /* type 4: Local APIC NMI, 6 byte */
    acpi_entry_hdr hdr;
    uint8_t  uid;                   /* 0xFF = mọi CPU */
    uint16_t flags;
    uint8_t  lint;                  /* LINT0 / LINT1 */
} __attribute__((packed)) acpi_madt_lapic_nmi;

typedef struct {                    /* type 5: Local APIC Address Override, 12 byte */
    acpi_entry_hdr hdr;
    uint16_t rsvd;
    uint64_t address;
} __attribute__((packed)) acpi_madt_lapic_override;

void          acpi_init(void);                    /* = phần "tìm bảng" của initiateACPI() */
void          acpi_dump(void);                    /* RSDP, danh sách bảng, FADT, MADT */
acpi_sdt_hdr *acpi_find(const char *sig);         /* = uacpi_table_find_by_signature */
acpi_madt    *acpi_madt_get(void);
