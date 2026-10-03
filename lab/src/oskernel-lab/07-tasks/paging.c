/* paging.c — VMM lab (Step 05).
 * Khác cavOS: cavOS TÁI DÙNG thẳng bảng của Limine (globalPagedir = P2V(cr3)).
 * Lab tự dựng PML4 mới rồi `mov cr3` để thấy tận mắt việc đổi address space.
 */
#include "paging.h"
#include "boot.h"
#include "pmm.h"
#include "serial.h"

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

static uint64_t *pml4_va;
static uint64_t  pml4_pa;
static uint64_t  old_pml4_pa;

static uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static void write_cr3(uint64_t v) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(v) : "memory");
}

static void invlpg(uint64_t va) {
    __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
}

static void zero_frame(uint64_t pa) {
    memset8(P2V(pa), 0, PAGE_SIZE);
}

/* index 9 bit cho từng tầng: PML4=47:39, PDPT=38:30, PD=29:21, PT=20:12 */
static uint64_t idx_of(uint64_t va, int level) {
    return (va >> (12 + 9 * (level - 1))) & 0x1FF;
}

void paging_init(void) {
    old_pml4_pa = read_cr3() & PTE_ADDR_MASK;

    pml4_pa = pmm_alloc();
    pml4_va = (uint64_t *)P2V(pml4_pa);

    /* Copy 512 entry = copy CON TRỎ tới các bảng con của Limine
     * -> giữ nguyên mapping kernel + HHDM, bảng con vẫn là của Limine. */
    const uint64_t *old_va = (const uint64_t *)P2V(old_pml4_pa);
    for (int i = 0; i < 512; i++)
        pml4_va[i] = old_va[i];

    write_cr3(pml4_pa);                     /* mov cr3 THẬT -> address space riêng */

    pmm_note_object("PML4 (new)", pml4_pa, pml4_pa + PAGE_SIZE);
    pmm_note_object("PML4 (old)", old_pml4_pa, old_pml4_pa + PAGE_SIZE);
}

/* Lấy bảng con của `table[index]`; chưa PRESENT thì cấp frame mới từ PMM (lazy). */
static uint64_t *next_table(uint64_t *table, uint64_t index, uint64_t flags,
                            uint64_t *out_pa, int *out_new) {
    if (!(table[index] & PTE_PRESENT)) {
        uint64_t pa = pmm_alloc();
        if (pa == 0)
            return 0;
        zero_frame(pa);
        table[index] = pa | PTE_PRESENT | PTE_RW | (flags & PTE_USER);
        *out_pa  = pa;
        *out_new = 1;
        return (uint64_t *)P2V(pa);
    }

    *out_pa  = table[index] & PTE_ADDR_MASK;
    *out_new = 0;
    return (uint64_t *)P2V(*out_pa);
}

/* Lab 0x07: vmap vào một PML4 bất kỳ (của task khác), như cavOS VirtualMapL(pagedir, ...). */
int vmap_in(uint64_t pml4, uint64_t va, uint64_t pa, uint64_t flags, struct vmap_info *out) {
    struct vmap_info info = {0, 0, 0, 0, 0, 0};
    uint64_t *top = (uint64_t *)P2V(pml4);

    uint64_t *pdpt = next_table(top, idx_of(va, 4), flags,
                                &info.pdpt_pa, &info.pdpt_new);
    if (pdpt == 0)
        return 0;

    uint64_t *pd = next_table(pdpt, idx_of(va, 3), flags, &info.pd_pa, &info.pd_new);
    if (pd == 0)
        return 0;

    uint64_t *pt = next_table(pd, idx_of(va, 2), flags, &info.pt_pa, &info.pt_new);
    if (pt == 0)
        return 0;

    pt[idx_of(va, 1)] = (pa & PTE_ADDR_MASK) | PTE_PRESENT | flags;
    invlpg(va);                              /* xoá TLB cho đúng VA vừa đổi */

    if (out != 0)
        *out = info;
    return 1;
}

int vmap(uint64_t va, uint64_t pa, uint64_t flags, struct vmap_info *out) {
    return vmap_in(pml4_pa, va, pa, flags, out);
}

uint64_t vresolve_in(uint64_t pml4, uint64_t va) {
    uint64_t e = ((const uint64_t *)P2V(pml4))[idx_of(va, 4)];
    if (!(e & PTE_PRESENT))
        return 0;

    const uint64_t *pdpt = (const uint64_t *)P2V(e & PTE_ADDR_MASK);
    e = pdpt[idx_of(va, 3)];
    if (!(e & PTE_PRESENT))
        return 0;
    if (e & PTE_PS)                          /* hugepage 1 GiB */
        return (e & PTE_ADDR_MASK) + (va & 0x3FFFFFFF);

    const uint64_t *pd = (const uint64_t *)P2V(e & PTE_ADDR_MASK);
    e = pd[idx_of(va, 2)];
    if (!(e & PTE_PRESENT))
        return 0;
    if (e & PTE_PS)                          /* hugepage 2 MiB (HHDM dùng cái này) */
        return (e & PTE_ADDR_MASK) + (va & 0x1FFFFF);

    const uint64_t *pt = (const uint64_t *)P2V(e & PTE_ADDR_MASK);
    e = pt[idx_of(va, 1)];
    if (!(e & PTE_PRESENT))
        return 0;

    return (e & PTE_ADDR_MASK) + (va & 0xFFF);
}

uint64_t vresolve(uint64_t va) { return vresolve_in(pml4_pa, va); }

/* = cavOS PageDirectoryAllocate(): 1 frame mới, chép nguyên 512 entry PML4 của task 0
 * ("model"). Nửa cao (kernel, HHDM, MMIO) dùng CHUNG bảng con; nửa thấp còn trống
 * để mỗi task tự map stack của nó ở cùng một VA. */
uint64_t paging_alloc_pagedir(uint64_t model) {
    uint64_t pa = pmm_alloc();
    if (pa == 0)
        return 0;
    uint64_t *out = (uint64_t *)P2V(pa);
    const uint64_t *m = (const uint64_t *)P2V(model);
    for (int i = 0; i < 512; i++)
        out[i] = m[i];
    return pa;
}

/* ~ cavOS PageDirectoryFree() + bỏ chính PML4: trả mọi frame + bảng của NỬA THẤP mà
 * task này tự tạo (entry khác với model), rồi trả frame PML4. Không động vào nửa cao. */
static void free_level(uint64_t table_pa, int level, int *frames, int *tables) {
    const uint64_t *t = (const uint64_t *)P2V(table_pa);
    for (int i = 0; i < 512; i++) {
        uint64_t e = t[i];
        if (!(e & PTE_PRESENT))
            continue;
        if (level == 1 || (e & PTE_PS)) {    /* lá: frame dữ liệu (stack) */
            pmm_free(e & PTE_ADDR_MASK);
            (*frames)++;
        } else {
            free_level(e & PTE_ADDR_MASK, level - 1, frames, tables);
        }
    }
    pmm_free(table_pa);
    (*tables)++;
}

void paging_free_pagedir(uint64_t pml4, uint64_t model, int *frames, int *tables) {
    const uint64_t *t = (const uint64_t *)P2V(pml4);
    const uint64_t *m = (const uint64_t *)P2V(model);
    for (int i = 0; i < 256; i++)
        if ((t[i] & PTE_PRESENT) && t[i] != m[i])
            free_level(t[i] & PTE_ADDR_MASK, 3, frames, tables);
    pmm_free(pml4);
}

uint64_t paging_read_cr3(void) { return read_cr3() & PTE_ADDR_MASK; }

static void walk_line(const char *name, uint64_t table_pa, uint64_t index) {
    serial_puts("       ");
    serial_puts(name);
    serial_puts(" @");
    serial_puthex(table_pa);
    serial_puts(" [idx ");
    serial_puthex(index);
    serial_puts("]\n");
}

void paging_dump_walk(const char *label, uint64_t va) {
    serial_puts("[walk] ");
    serial_puts(label);
    serial_puts(" VA=");
    serial_puthex(va);
    serial_putc('\n');

    walk_line("PML4", pml4_pa, idx_of(va, 4));

    uint64_t e = pml4_va[idx_of(va, 4)];
    if (!(e & PTE_PRESENT)) {
        serial_puts("       PML4 entry not present\n");
        return;
    }

    uint64_t pdpt_pa = e & PTE_ADDR_MASK;
    walk_line("PDPT", pdpt_pa, idx_of(va, 3));

    const uint64_t *pdpt = (const uint64_t *)P2V(pdpt_pa);
    e = pdpt[idx_of(va, 3)];
    if (!(e & PTE_PRESENT)) {
        serial_puts("       PDPT entry not present\n");
        return;
    }
    if (e & PTE_PS) {
        serial_puts("       PDPT entry = 1GB hugepage -> PA ");
        serial_puthex(e & PTE_ADDR_MASK);
        serial_putc('\n');
        return;
    }

    uint64_t pd_pa = e & PTE_ADDR_MASK;
    walk_line("PD  ", pd_pa, idx_of(va, 2));

    const uint64_t *pd = (const uint64_t *)P2V(pd_pa);
    e = pd[idx_of(va, 2)];
    if (!(e & PTE_PRESENT)) {
        serial_puts("       PD entry not present\n");
        return;
    }
    if (e & PTE_PS) {
        serial_puts("       PD entry = 2MB hugepage -> PA ");
        serial_puthex(e & PTE_ADDR_MASK);
        serial_putc('\n');
        return;
    }

    uint64_t pt_pa = e & PTE_ADDR_MASK;
    walk_line("PT  ", pt_pa, idx_of(va, 1));
}

uint64_t paging_leaf(uint64_t va, int *level) {
    uint64_t e = pml4_va[idx_of(va, 4)];
    if (!(e & PTE_PRESENT))
        return 0;
    const uint64_t *pdpt = (const uint64_t *)P2V(e & PTE_ADDR_MASK);
    e = pdpt[idx_of(va, 3)];
    if (!(e & PTE_PRESENT))
        return 0;
    if (e & PTE_PS) { *level = 3; return e; }
    const uint64_t *pd = (const uint64_t *)P2V(e & PTE_ADDR_MASK);
    e = pd[idx_of(va, 2)];
    if (!(e & PTE_PRESENT))
        return 0;
    if (e & PTE_PS) { *level = 2; return e; }
    const uint64_t *pt = (const uint64_t *)P2V(e & PTE_ADDR_MASK);
    e = pt[idx_of(va, 1)];
    if (!(e & PTE_PRESENT))
        return 0;
    *level = 1;
    return e;
}

uint64_t paging_pml4_pa(void)     { return pml4_pa; }
uint64_t paging_old_pml4_pa(void) { return old_pml4_pa; }
