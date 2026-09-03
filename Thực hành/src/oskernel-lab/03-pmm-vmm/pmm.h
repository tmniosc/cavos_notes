/* pmm.h — Physical Memory Manager: bitmap 1 bit / frame 4 KiB (Step 04). */
#pragma once
#include <stdint.h>

void     pmm_init(void);
uint64_t pmm_alloc(void);           /* trả PA frame, 0 nếu hết */
void     pmm_free(uint64_t pa);

uint64_t pmm_total_blocks(void);    /* tổng frame theo dõi (từ mmTotal) */
uint64_t pmm_bitmap_bytes(void);
uint64_t pmm_bitmap_pa(void);
uint64_t pmm_free_count(void);

/* Ghi nhận 1 OBJECT (bitmap, PML4, PDPT...) để in dòng con trong bảng dump. */
void pmm_note_object(const char *name, uint64_t pa_begin, uint64_t pa_end);
void pmm_dump_map(void);
