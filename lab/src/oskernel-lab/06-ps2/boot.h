/* boot.h — Limine requests + HHDM + tiện ích chung (Step 00/02; Lab 0x05 thêm RSDP + SMP). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../limine.h"

#define PAGE_SIZE 4096ULL
#define DivRoundUp(a, b) (((a) + (b) - 1) / (b))

extern uint64_t hhdmOffset;

/* PA <-> VA qua HHDM: mọi RAM vật lý đều nhìn thấy ở nửa cao ([[HHDM]]). */
#define P2V(pa) ((void *)((uint64_t)(pa) + hhdmOffset))
#define V2P(va) ((uint64_t)(va) - hhdmOffset)

void boot_init(void);
struct limine_memmap_response       *boot_memmap(void);
struct limine_kernel_address_response *boot_kaddr(void);
struct limine_smp_response          *boot_smp(void);

/* Như cavOS bootloader.c: base revision 2 trả RSDP là VA trong HHDM -> trừ hhdmOffset. */
uint64_t boot_rsdp_raw(void);       /* giá trị Limine đưa, chưa sửa */
uint64_t boot_rsdp_phys(void);      /* PA của RSDP */

void memset8(void *dst, uint8_t val, uint64_t n);
