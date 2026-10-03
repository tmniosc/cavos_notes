/* boot.c — gom các Limine request lab cần rồi lưu hhdmOffset dùng chung.
 * Lab 0x05 thêm 2 request cavOS cũng có (entry/bootloader.c): RSDP và SMP. */
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

LIMINE_REQUEST static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST, .revision = 0};

LIMINE_REQUEST static volatile struct limine_smp_request smp_request = {
    .id = LIMINE_SMP_REQUEST, .revision = 0};

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

/* Lab 0x07: GCC được phép tự sinh lời gọi memcpy/memset khi chép struct lớn
 * (AsmPassedInterrupt 176 byte) dù có -ffreestanding -> kernel phải tự có 2 hàm này. */
/* Viết bằng rep movsb/stosb: nếu viết vòng lặp C, -O2 có thể "nhận ra" vòng lặp và
 * biến nó thành lời gọi memcpy -> hàm tự gọi chính nó. */
void *memcpy(void *dst, const void *src, size_t n) {
    void *d = dst;
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(src), "+c"(n) : : "memory");
    return dst;
}

void *memset(void *dst, int val, size_t n) {
    void *d = dst;
    __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(val) : "memory");
    return dst;
}

struct limine_smp_response *boot_smp(void) {
    return smp_request.response;
}

uint64_t boot_rsdp_raw(void) {
    return rsdp_request.response ? (uint64_t)rsdp_request.response->address : 0;
}

/* cavOS: bootloader.rsdp = (size_t)rsdp_response->address - bootloader.hhdmOffset;
 * ("todo: revision >= 3 and it's not virtual!") — base revision 3 trở đi Limine trả PA. */
uint64_t boot_rsdp_phys(void) {
    uint64_t a = boot_rsdp_raw();
    return a >= hhdmOffset ? a - hhdmOffset : a;
}
