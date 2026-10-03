/* paging.h — VMM: tự dựng PML4 mới + mov cr3, vmap/vresolve 4 tầng (Step 05). */
#pragma once
#include <stdint.h>

#define PTE_PRESENT (1ULL << 0)
#define PTE_RW      (1ULL << 1)
#define PTE_USER    (1ULL << 2)
#define PTE_PWT     (1ULL << 3)   /* write-through */
#define PTE_PCD     (1ULL << 4)   /* cache disable: PCD+PWT = PAT entry 3 = UC (Lab 0x05, MMIO) */
#define PTE_PS      (1ULL << 7)   /* hugepage: 2 MiB ở tầng PD */

/* Kết quả 1 lần vmap: 3 bảng trung gian nằm PA nào, cái nào vừa cấp mới. */
struct vmap_info {
    uint64_t pdpt_pa;
    uint64_t pd_pa;
    uint64_t pt_pa;
    int pdpt_new;
    int pd_new;
    int pt_new;
};

void     paging_init(void);                  /* PML4 mới + copy 512 entry + mov cr3 */
int      vmap(uint64_t va, uint64_t pa, uint64_t flags, struct vmap_info *out);
uint64_t vresolve(uint64_t va);              /* walk ngược VA -> PA, 0 nếu chưa map */
void     paging_dump_walk(const char *label, uint64_t va);

/* Lab 0x05: trả entry lá (PTE, hoặc PDE/PDPTE nếu là hugepage) của VA, 0 nếu chưa map.
 * *level = 1 (4 KiB), 2 (2 MiB), 3 (1 GiB). Dùng để xem cờ PCD/PWT của một mapping. */
uint64_t paging_leaf(uint64_t va, int *level);

uint64_t paging_pml4_pa(void);               /* PML4 mới (lab tự cấp) */
uint64_t paging_old_pml4_pa(void);           /* PML4 cũ của Limine (CR3 lúc boot) */
