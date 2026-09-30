/* pmm.c — bitmap PMM tự host trong RAM, truy cập qua HHDM (Step 04).
 * Trình tự đúng như cavOS: đóng tất (0xff) -> mở USABLE -> đóng non-USABLE
 * -> tự đánh dấu chính bitmap.
 */
#include "pmm.h"
#include <stddef.h>
#include "boot.h"
#include "serial.h"

static uint8_t *bitmap;          /* VA (qua HHDM) của bitmap */
static uint64_t bitmap_pa;
static uint64_t blocks;          /* tổng frame theo dõi */
static uint64_t bytes;           /* cỡ bitmap */

/* --- bit helpers --- */

static void bm_set(uint64_t i)   { bitmap[i / 8] |= (uint8_t)(1u << (i % 8)); }
static void bm_clear(uint64_t i) { bitmap[i / 8] &= (uint8_t)~(1u << (i % 8)); }
static int  bm_test(uint64_t i)  { return (bitmap[i / 8] >> (i % 8)) & 1; }

/* Đánh dấu [base, base+len) là used(1) / free(0).
 * GUARD: entry MMIO địa chỉ rất cao (vd 0xfd00000000) cho index vượt `blocks`
 * -> phải chặn, không thì ghi tràn ra ngoài bitmap. */
static void bm_mark(uint64_t base, uint64_t len, int used) {
    uint64_t first = base / PAGE_SIZE;
    uint64_t count = DivRoundUp(len, PAGE_SIZE);

    for (uint64_t i = 0; i < count; i++) {
        if (first + i >= blocks)
            break;
        if (used)
            bm_set(first + i);
        else
            bm_clear(first + i);
    }
}

void pmm_init(void) {
    struct limine_memmap_response *mm = boot_memmap();
    if (mm == NULL)
        return;

    /* 1. mmTotal kiểu cavOS bootloader.c: CỘNG DỒN length mọi vùng != RESERVED.
     *    KHÔNG lấy "địa chỉ cuối cao nhất" — lỗ MMIO sẽ làm bitmap phình to. */
    uint64_t mmTotal = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++)
        if (mm->entries[i]->type != LIMINE_MEMMAP_RESERVED)
            mmTotal += mm->entries[i]->length;

    blocks = DivRoundUp(mmTotal, PAGE_SIZE);
    bytes  = DivRoundUp(blocks, 8);

    /* 2. Con gà - quả trứng: allocator đầu tiên phải tự tìm chỗ cho chính nó.
     *    Đặt bitmap vào vùng USABLE đầu tiên đủ chỗ. */
    bitmap_pa = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bytes) {
            bitmap_pa = e->base;
            break;
        }
    }
    if (bitmap_pa == 0)
        return;

    bitmap = (uint8_t *)P2V(bitmap_pa);

    /* 3. Đóng TẤT CẢ trước — frame nằm trong "address gap" sẽ mãi mãi = used. */
    memset8(bitmap, 0xFF, bytes);

    /* 4. Mở các vùng USABLE. */
    for (uint64_t i = 0; i < mm->entry_count; i++)
        if (mm->entries[i]->type == LIMINE_MEMMAP_USABLE)
            bm_mark(mm->entries[i]->base, mm->entries[i]->length, 0);

    /* 5. Đóng lại các vùng KHÔNG usable (BOOT_REC coi như chưa reclaim). */
    for (uint64_t i = 0; i < mm->entry_count; i++)
        if (mm->entries[i]->type != LIMINE_MEMMAP_USABLE)
            bm_mark(mm->entries[i]->base, mm->entries[i]->length, 1);

    /* 6. Tự đánh dấu chính bitmap — làm tròn LÊN, kể cả frame đuôi dùng dở. */
    bm_mark(bitmap_pa, bytes, 1);

    pmm_note_object("PMM bitmap", bitmap_pa, bitmap_pa + bytes);
}

uint64_t pmm_alloc(void) {
    for (uint64_t i = 0; i < blocks; i++) {
        if (!bm_test(i)) {
            bm_set(i);
            return i * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free(uint64_t pa) {
    uint64_t i = pa / PAGE_SIZE;
    if (i < blocks)
        bm_clear(i);
}

uint64_t pmm_total_blocks(void) { return blocks; }
uint64_t pmm_bitmap_bytes(void) { return bytes; }
uint64_t pmm_bitmap_pa(void)    { return bitmap_pa; }

uint64_t pmm_free_count(void) {
    uint64_t n = 0;
    for (uint64_t i = 0; i < blocks; i++)
        if (!bm_test(i))
            n++;
    return n;
}

/* ===================== bảng pmm_dump_map ===================== */

#define MAX_OBJECTS 16

struct pmm_object {
    const char *name;
    uint64_t begin;
    uint64_t end;
};

static struct pmm_object objects[MAX_OBJECTS];
static uint64_t object_count;

void pmm_note_object(const char *name, uint64_t pa_begin, uint64_t pa_end) {
    if (object_count >= MAX_OBJECTS)
        return;
    objects[object_count].name  = name;
    objects[object_count].begin = pa_begin;
    objects[object_count].end   = pa_end;
    object_count++;
}

/* độ rộng cột — khớp đúng file outputs/lab-0x03-run.txt */
#define W_ADDR   20
#define W_TYPE   10
#define W_ALLOC   7
#define W_STORES 16
#define W_SIZE   12
#define W_INNER  (W_ADDR * 6 + W_TYPE + W_ALLOC + W_STORES + W_SIZE + 9)

static const char *type_name(uint64_t t) {
    switch (t) {
    case LIMINE_MEMMAP_USABLE:                 return "USABLE";
    case LIMINE_MEMMAP_RESERVED:               return "RESERVED";
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return "ACPI_REC";
    case LIMINE_MEMMAP_ACPI_NVS:               return "ACPI_NVS";
    case LIMINE_MEMMAP_BAD_MEMORY:             return "BAD_MEM";
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "BOOT_REC";
    case LIMINE_MEMMAP_KERNEL_AND_MODULES:     return "KERNEL";
    case LIMINE_MEMMAP_FRAMEBUFFER:            return "FRAMEBUF";
    default:                                   return "UNKNOWN";
    }
}

/* RAM thật -> có VA qua HHDM. MMIO/RESERVED để '-' cho khỏi nhầm "có VA = là RAM". */
static int is_ram(uint64_t t) {
    return t == LIMINE_MEMMAP_USABLE ||
           t == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
           t == LIMINE_MEMMAP_KERNEL_AND_MODULES ||
           t == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
           t == LIMINE_MEMMAP_ACPI_NVS;
}

static int is_allocatable(uint64_t t) {
    return t == LIMINE_MEMMAP_USABLE || t == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE;
}

static void cell_addr(uint64_t v, int show) {
    serial_puts("| ");
    if (show)
        serial_puthex(v);
    else
        serial_puts_pad("-", 18);
    serial_putc(' ');
}

static void cell_text(const char *s, uint64_t width) {
    serial_puts("| ");
    serial_puts_pad(s, width);
    serial_putc(' ');
}

static void row_separator(void) {
    serial_putc('+');
    for (int i = 0; i < 6; i++) {
        serial_repeat('-', W_ADDR);
        serial_putc('+');
    }
    serial_repeat('-', W_TYPE);   serial_putc('+');
    serial_repeat('-', W_ALLOC);  serial_putc('+');
    serial_repeat('-', W_STORES); serial_putc('+');
    serial_repeat('-', W_SIZE);   serial_putc('+');
    serial_putc('\n');
}

static void row(uint64_t pa_b, uint64_t pa_e,
                int has_hhdm, uint64_t va_b, uint64_t va_e,
                int has_kern, uint64_t kv_b, uint64_t kv_e,
                const char *type, int is_obj, char alloc,
                const char *stores) {
    cell_addr(pa_b, 1);
    cell_addr(pa_e, 1);
    cell_addr(va_b, has_hhdm);
    cell_addr(va_e, has_hhdm);
    cell_addr(kv_b, has_kern);
    cell_addr(kv_e, has_kern);

    cell_text(is_obj ? " (obj)" : type, 8);

    serial_puts("|   ");
    serial_putc(alloc);
    serial_puts("   ");

    cell_text(stores, 14);

    serial_puts("| ");
    serial_putdec_right((pa_e - pa_b) / 1024, 8);
    serial_puts(" K |\n");
}

static void gap_row(void) {
    static const char text[] = ".. address gap (not backed by RAM/device) ..";
    uint64_t len = sizeof(text) - 1;
    uint64_t left = (W_INNER - len) / 2;

    serial_putc('|');
    serial_spaces(left);
    serial_puts(text);
    serial_spaces(W_INNER - len - left);
    serial_puts("|\n");
}

void pmm_dump_map(void) {
    struct limine_memmap_response *mm = boot_memmap();
    struct limine_kernel_address_response *ka = boot_kaddr();
    if (mm == NULL)
        return;

    serial_puts("\n[map] PMM physical memory layout (region rows + sub-rows for objects)\n");
    row_separator();

    cell_text("PA begin", 18);
    cell_text("PA end", 18);
    cell_text("VA-HHDM begin", 18);
    cell_text("VA-HHDM end", 18);
    cell_text("VA-kernel begin", 18);
    cell_text("VA-kernel end", 18);
    cell_text("type", 8);
    cell_text("alloc", 5);
    cell_text("stores", 14);
    cell_text("size", 10);
    serial_puts("|\n");
    row_separator();

    uint64_t prev_end = 0;

    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        uint64_t b  = e->base;
        uint64_t en = e->base + e->length;

        if (i > 0 && b > prev_end)
            gap_row();                    /* đất trống: không RAM lẫn device */

        int ram  = is_ram(e->type);
        int kern = (e->type == LIMINE_MEMMAP_KERNEL_AND_MODULES && ka != NULL);

        row(b, en,
            ram,  b + hhdmOffset, en + hhdmOffset,
            kern, kern ? ka->virtual_base : 0,
            kern ? ka->virtual_base + e->length : 0,
            type_name(e->type), 0,
            is_allocatable(e->type) ? 'Y' : 'n',
            e->type == LIMINE_MEMMAP_FRAMEBUFFER ? "framebuffer" : "(region)");

        /* dòng con: object nào nằm trong vùng này */
        for (uint64_t o = 0; o < object_count; o++) {
            if (objects[o].begin < b || objects[o].begin >= en)
                continue;

            char label[20];
            int  n = 0;
            label[n++] = '>';
            label[n++] = ' ';
            for (const char *s = objects[o].name; *s != 0 && n < 19; s++)
                label[n++] = *s;
            label[n] = 0;

            row(objects[o].begin, objects[o].end,
                ram, objects[o].begin + hhdmOffset, objects[o].end + hhdmOffset,
                0, 0, 0,
                "", 1, '.', label);
        }

        prev_end = en;
    }

    row_separator();
    serial_puts("region row = 1 memmap entry; sub-row starting with > = 1 OBJECT inside that region.\n");
    serial_puts("PDPT/PD/PT testVA = tables LAB built for the test mapping 0x600000000000.\n");
    serial_puts("HHDM/kernel sub-tables are built by Limine (PML4 new shares them) -> not listed.\n");
    serial_puts("VA-HHDM = PA + HHDM (RAM only); VA-kernel only for kernel image.\n");
}
