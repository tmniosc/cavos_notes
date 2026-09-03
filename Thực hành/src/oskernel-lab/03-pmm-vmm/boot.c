/* boot.c — gom 3 Limine request lab cần rồi lưu hhdmOffset dùng chung. */
#include "boot.h"

#define LIMINE_REQUEST __attribute__((used, section(".limine_requests")))

LIMINE_REQUEST static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0};

LIMINE_REQUEST static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0};

LIMINE_REQUEST static volatile struct limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0};

LIMINE_REQUEST static volatile struct limine_paging_mode_request paging_request = {
    .id = LIMINE_PAGING_MODE_REQUEST, .revision = 0,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL};

uint64_t hhdmOffset = 0;

void boot_init(void) {
    if (hhdm_request.response != NULL)
        hhdmOffset = hhdm_request.response->offset;
}

struct limine_memmap_response *boot_memmap(void) {
    return memmap_request.response;
}

struct limine_kernel_address_response *boot_kaddr(void) {
    return kaddr_request.response;
}

void memset8(void *dst, uint8_t val, uint64_t n) {
    uint8_t *p = (uint8_t *)dst;
    for (uint64_t i = 0; i < n; i++)
        p[i] = val;
}
