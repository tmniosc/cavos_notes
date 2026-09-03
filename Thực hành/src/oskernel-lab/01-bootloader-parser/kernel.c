/* Lab 0x01 — Bootloader Parser (Step 02).
 * Khai 4 Limine request, đọc response, in ra serial: paging mode / HHDM /
 * kernel address / memmap. Xem [[Request-Response Mechanism]].
 */
#include <stddef.h>
#include "../limine.h"
#include "serial.h"

LIMINE_BASE_REVISION(2)

/* Request phải nằm trong .limine_requests (linker.ld KEEP) và `used` để
 * -O2 không loại bỏ biến static không ai đọc. */
#define LIMINE_REQUEST __attribute__((used, section(".limine_requests")))

LIMINE_REQUEST static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0};

LIMINE_REQUEST static volatile struct limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0};

LIMINE_REQUEST static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0};

LIMINE_REQUEST static volatile struct limine_paging_mode_request paging_request = {
    .id = LIMINE_PAGING_MODE_REQUEST, .revision = 0,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL};

static const char *memmap_type_name(uint64_t type) {
    switch (type) {
    case LIMINE_MEMMAP_USABLE:                 return "USABLE";
    case LIMINE_MEMMAP_RESERVED:               return "RESERVED";
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return "ACPI_RECLAIM";
    case LIMINE_MEMMAP_ACPI_NVS:               return "ACPI_NVS";
    case LIMINE_MEMMAP_BAD_MEMORY:             return "BAD_MEM";
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "BOOT_RECLAIM";
    case LIMINE_MEMMAP_KERNEL_AND_MODULES:     return "KERNEL_MODS";
    case LIMINE_MEMMAP_FRAMEBUFFER:            return "FRAMEBUFFER";
    default:                                   return "UNKNOWN";
    }
}

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    serial_puts("=== Lab 0x01 - Bootloader Parser (Step 02) ===\n");

    /* 1. paging mode — 4-level hay 5-level */
    if (paging_request.response != NULL) {
        serial_puts("[paging] mode = ");
        serial_puts(paging_request.response->mode == LIMINE_PAGING_MODE_X86_64_4LVL
                        ? "4-level\n" : "5-level\n");
    } else {
        serial_puts("[paging] no response\n");
    }

    /* 2. HHDM offset — cửa sổ nhìn toàn bộ RAM vật lý ở nửa cao ([[HHDM]]) */
    if (hhdm_request.response != NULL) {
        serial_puts("[hhdm]   offset = ");
        serial_puthex(hhdm_request.response->offset);
        serial_putc('\n');
    } else {
        serial_puts("[hhdm]   no response\n");
    }

    /* 3. kernel address — virt cố định, phys đổi mỗi lần boot (KASLR) */
    if (kaddr_request.response != NULL) {
        serial_puts("[kernel] virt = ");
        serial_puthex(kaddr_request.response->virtual_base);
        serial_puts("\n         phys = ");
        serial_puthex(kaddr_request.response->physical_base);
        serial_putc('\n');
    } else {
        serial_puts("[kernel] no response\n");
    }

    /* 4. memmap — bản đồ vùng nhớ vật lý */
    if (memmap_request.response != NULL) {
        struct limine_memmap_response *mm = memmap_request.response;
        uint64_t usable = 0, non_reserved = 0;

        serial_puts("[memmap]                                         ; ");
        serial_putdec(mm->entry_count);
        serial_puts(" entries\n");

        for (uint64_t i = 0; i < mm->entry_count; i++) {
            struct limine_memmap_entry *e = mm->entries[i];

            serial_puts("  [");
            serial_putdec(i);
            serial_puts("] base=");
            serial_puthex(e->base);
            serial_puts(" len=");
            serial_puthex(e->length);
            serial_putc(' ');
            serial_puts(memmap_type_name(e->type));
            serial_putc('\n');

            if (e->type == LIMINE_MEMMAP_USABLE)
                usable += e->length;
            if (e->type != LIMINE_MEMMAP_RESERVED)
                non_reserved += e->length;   /* = mmTotal kiểu cavOS bootloader.c */
        }

        serial_puts("  -- USABLE = ");
        serial_putdec(usable / (1024 * 1024));
        serial_puts(" MiB / non-RES = ");
        serial_putdec(non_reserved / (1024 * 1024));
        serial_puts(" MiB\n");
    } else {
        serial_puts("[memmap] no response\n");
    }

    serial_puts("[done] halt.\n");
    halt();
}
