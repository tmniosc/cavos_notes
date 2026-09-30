/* Lab 0x03 — PMM + VMM (Step 04 + Step 05).
 * Dựng bitmap PMM từ memmap, rồi tự dựng PML4 mới + mov cr3, vmap 4 tầng
 * và chứng minh aliasing: ghi qua VA mới, đọc qua VA HHDM của cùng PA.
 */
#include "../limine.h"
#include "boot.h"
#include "pmm.h"
#include "paging.h"
#include "serial.h"

LIMINE_BASE_REVISION(2)

#define TEST_VA   0x0000600000000000ULL
#define TEST_MAGIC 0xdeadbeefcafebabeULL

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    boot_init();

    serial_puts("=== Lab 0x03 - PMM + VMM (Step 04 + 05) ===\n\n");

    serial_puts("[init] hhdm offset = ");
    serial_puthex(hhdmOffset);
    serial_puts("\n\n");

    /* ---------------- PMM (Step 04) ---------------- */
    pmm_init();

    serial_puts("[pmm] total frames = ");
    serial_putdec(pmm_total_blocks());
    serial_puts("  bitmap bytes = ");
    serial_putdec(pmm_bitmap_bytes());
    serial_puts("\n[pmm] bitmap at PA = ");
    serial_puthex(pmm_bitmap_pa());
    serial_puts("  (block #");
    serial_putdec(pmm_bitmap_pa() / PAGE_SIZE);
    serial_puts(")  VA = ");
    serial_puthex((uint64_t)P2V(pmm_bitmap_pa()));
    serial_putc('\n');

    uint64_t free_start = pmm_free_count();
    serial_puts("[pmm] free frames  = ");
    serial_putdec(free_start);
    serial_putc('\n');

    uint64_t a1 = pmm_alloc();
    uint64_t a2 = pmm_alloc();
    serial_puts("[pmm] alloc #1 PA  = ");
    serial_puthex(a1);
    serial_puts("\n[pmm] alloc #2 PA  = ");
    serial_puthex(a2);
    serial_putc('\n');

    uint64_t free_after_alloc = pmm_free_count();
    serial_puts("[pmm] free -2 = ");
    serial_putdec(free_after_alloc);
    serial_puts(free_after_alloc == free_start - 2 ? "  OK\n" : "  MISMATCH\n");

    pmm_free(a1);
    pmm_free(a2);
    uint64_t free_after_free = pmm_free_count();
    serial_puts("[pmm] free after free = ");
    serial_putdec(free_after_free);
    serial_puts(free_after_free == free_start ? "  OK (back to start)\n\n"
                                              : "  MISMATCH\n\n");

    /* ---------------- VMM (Step 05) ---------------- */
    paging_init();
    serial_puts("[vmm] built new PML4 + mov cr3\n");
    serial_puts("[vmm] our pml4 (VA) = ");
    serial_puthex((uint64_t)P2V(paging_pml4_pa()));
    serial_putc('\n');

    /* Walk thật 2 VA có sẵn để thấy bảng con do Limine dựng nằm đâu. */
    paging_dump_walk("HHDM   ", hhdmOffset);
    paging_dump_walk("kernel ", 0xffffffff80000000ULL);

    uint64_t data_pa = pmm_alloc();
    struct vmap_info info;

    if (!vmap(TEST_VA, data_pa, PTE_RW, &info)) {
        serial_puts("[vmm] vmap FAILED\n");
        halt();
    }

    pmm_note_object("PDPT testVA", info.pdpt_pa, info.pdpt_pa + PAGE_SIZE);
    pmm_note_object("PD testVA",   info.pd_pa,   info.pd_pa + PAGE_SIZE);
    pmm_note_object("PT testVA",   info.pt_pa,   info.pt_pa + PAGE_SIZE);

    serial_puts("[vmm] map VA ");
    serial_puthex(TEST_VA);
    serial_puts(" -> PA ");
    serial_puthex(data_pa);
    serial_puts("\n[vmm] tables for VA: PDPT=");
    serial_puthex(info.pdpt_pa);
    serial_puts(info.pdpt_new ? " (new) PD=" : " (reuse) PD=");
    serial_puthex(info.pd_pa);
    serial_puts(info.pd_new ? " (new) PT=" : " (reuse) PT=");
    serial_puthex(info.pt_pa);
    serial_puts(info.pt_new ? " (new)\n" : " (reuse)\n");

    /* ---------------- bảng bản đồ vật lý ---------------- */
    pmm_dump_map();

    /* ---------------- chứng minh aliasing ---------------- */
    volatile uint64_t *via_new_va = (volatile uint64_t *)TEST_VA;
    volatile uint64_t *via_hhdm   = (volatile uint64_t *)P2V(data_pa);

    *via_new_va = TEST_MAGIC;                /* ghi qua VA mới map */

    serial_puts("[vmm] write via new VA, read via HHDM = ");
    serial_puthex(*via_hhdm);                /* đọc qua VA HHDM của CÙNG PA */
    serial_puts(*via_hhdm == TEST_MAGIC ? "  OK (same PA!)\n" : "  MISMATCH\n");

    uint64_t resolved = vresolve(TEST_VA);
    serial_puts("[vmm] vresolve(VA)  = ");
    serial_puthex(resolved);
    serial_puts(resolved == data_pa ? "  OK\n" : "  MISMATCH\n");

    serial_puts("\n[done] halt.\n");
    halt();
}
