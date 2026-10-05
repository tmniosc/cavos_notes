/* elf.c — = cavOS utilities/elf.c + phần stackGenerateUser() của multitasking/stack.c (Bài 18).
 *
 * Giống cavOS:
 *  - đọc cả file vào bộ nhớ kernel (cavOS: fsKernelOpen + fsRead; lab: fsReadFile của Lab 0x09);
 *  - elf_check_file: magic 7F 'E' 'L' 'F', ELFCLASS64, e_machine = x86-64;
 *  - PML4 mới; với mỗi PT_LOAD: map đủ trang (PF_USER | PF_RW), chép p_filesz byte từ file,
 *    xoá phần p_memsz - p_filesz (.bss);
 *  - task mới với RIP = e_entry; stack user theo ABI System V: argc, argv[], 0, envp[], 0, auxv
 *    (cùng danh sách AT_* và cùng thứ tự đẩy như stackGenerateUser).
 * Khác cavOS (ghi trên trang lab):
 *  - chép vào trang của task mới qua HHDM, không đổi CR3 (cavOS: ChangePageDirectory);
 *  - chuỗi argv/envp và 16 byte AT_RANDOM đặt ở đỉnh stack (cavOS: trong heap của task);
 *  - không hỗ trợ PT_INTERP (chương trình động) và ET_DYN;
 *  - kiểm từng PT_LOAD nằm trọn trong nửa user (p_vaddr + p_memsz <= 0x800000000000).
 */
#include "elf.h"
#include "boot.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "vfs.h"

static uint8_t fileBuf[512 * 1024] __attribute__((aligned(4096)));

static void hx(uint64_t v) { serial_puthex_short(v); }
static uint64_t slenk(const char *s) { uint64_t n = 0; while (s[n]) n++; return n; }

/* ghi n byte vào VA của một PML4 khác, qua HHDM, từng trang một */
static void copyToUser(uint64_t pml4, uint64_t va, const uint8_t *src, uint64_t n, int zero) {
    while (n) {
        uint64_t pa = vresolve_in(pml4, va);
        uint64_t chunk = PAGE_SIZE - (va & 0xFFF);
        if (chunk > n) chunk = n;
        uint8_t *dst = (uint8_t *)P2V(pa);
        for (uint64_t i = 0; i < chunk; i++) dst[i] = zero ? 0 : src[i];
        if (!zero) src += chunk;
        va += chunk;
        n -= chunk;
    }
}

static void push64(uint64_t pml4, uint64_t *rsp, uint64_t v) {
    *rsp -= 8;
    copyToUser(pml4, *rsp, (const uint8_t *)&v, 8, 0);
}

static uint64_t peek64(uint64_t pml4, uint64_t va) {
    return *(uint64_t *)P2V(vresolve_in(pml4, va));
}

static int elf_check_file(const Elf64_Ehdr *h) {
    if (h->e_ident[0] != 0x7F || h->e_ident[1] != 'E' || h->e_ident[2] != 'L' || h->e_ident[3] != 'F') {
        serial_puts("[elf] bad magic\n");
        return 0;
    }
    if (h->e_ident[4] != 2 || h->e_machine != 0x3E) {
        serial_puts("[elf] Architecture is not supported.\n");
        return 0;
    }
    return 1;
}

/* = cavOS elfProcessLoad() */
static int elfProcessLoad(const Elf64_Phdr *p, const uint8_t *out, uint64_t pml4) {
    if (p->p_vaddr + p->p_memsz > USER_STACK_BOTTOM - USER_STACK_PAGES * PAGE_SIZE ||
        p->p_vaddr + p->p_memsz < p->p_vaddr) {
        serial_puts("[elf]   segment outside the user half, refused\n");
        return 0;
    }
    uint64_t start = p->p_vaddr & ~0xFFFULL;
    uint64_t pages = DivRoundUp((p->p_vaddr - start) + p->p_memsz, PAGE_SIZE);
    uint64_t fresh = 0;
    for (uint64_t j = 0; j < pages; j++) {
        uint64_t va = start + j * PAGE_SIZE;
        if (vresolve_in(pml4, va))
            continue;                                     /* trang đã có (đoạn trước dùng chung) */
        uint64_t pa = pmm_alloc();
        memset(P2V(pa), 0, PAGE_SIZE);
        vmap_in(pml4, va, pa, PTE_USER | PTE_RW, 0);      /* cavOS: PF_USER | PF_RW */
        fresh++;
    }
    copyToUser(pml4, p->p_vaddr, out + p->p_offset, p->p_filesz, 0);
    if (p->p_memsz > p->p_filesz)                         /* .bss */
        copyToUser(pml4, p->p_vaddr + p->p_filesz, 0, p->p_memsz - p->p_filesz, 1);

    serial_puts("[elf]   PT_LOAD offset ");
    hx(p->p_offset);
    serial_puts(" vaddr ");
    hx(p->p_vaddr);
    serial_puts(" filesz ");
    hx(p->p_filesz);
    serial_puts(" memsz ");
    hx(p->p_memsz);
    serial_puts(" flags ");
    serial_putc(p->p_flags & 4 ? 'R' : '-');
    serial_putc(p->p_flags & 2 ? 'W' : '-');
    serial_putc(p->p_flags & 1 ? 'X' : '-');
    serial_puts(" -> ");
    serial_putdec(pages);
    serial_puts(" page(s), ");
    serial_putdec(fresh);
    serial_puts(" new, ");
    serial_putdec(p->p_memsz - p->p_filesz);
    serial_puts(" byte(s) zeroed\n");
    return 1;
}

Task *elfExecute(const char *path, uint32_t argc, const char **argv, uint32_t envc, const char **envv) {
    long size = fsReadFile(path, fileBuf, sizeof(fileBuf));
    if (size < 0) {
        serial_puts("[elf] Could not open ");
        serial_puts(path);
        serial_putc('\n');
        return 0;
    }
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)fileBuf;
    serial_puts("[elf] ");
    serial_puts(path);
    serial_puts(": ");
    serial_putdec(size);
    serial_puts(" bytes, type ");
    serial_putdec(eh->e_type);
    serial_puts(eh->e_type == 2 ? " (ET_EXEC)" : "");
    serial_puts(", machine ");
    hx(eh->e_machine);
    serial_puts(", entry ");
    hx(eh->e_entry);
    serial_puts(", ");
    serial_putdec(eh->e_phnum);
    serial_puts(" program headers of ");
    serial_putdec(eh->e_phentsize);
    serial_puts(" bytes at offset ");
    hx(eh->e_phoff);
    serial_putc('\n');
    if (!elf_check_file(eh))
        return 0;

    uint64_t pml4 = paging_alloc_pagedir(firstTask->pagedir);
    uint64_t lowest = 0;
    for (int i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr *p = (const Elf64_Phdr *)(fileBuf + eh->e_phoff + i * eh->e_phentsize);
        if (p->p_type != PT_LOAD)
            continue;
        if (!elfProcessLoad(p, fileBuf, pml4))
            return 0;
        if (!lowest || p->p_vaddr < lowest)
            lowest = p->p_vaddr;
    }

    Task *t = taskCreateUserBlank(eh->e_entry, pml4, argv[0]);

    /* ---- stackGenerateUser: chuỗi + AT_RANDOM ở đỉnh stack, rồi auxv, envp, argv, argc ---- */
    uint64_t top = t->registers.usermode_rsp;              /* = USER_STACK_BOTTOM */
    uint64_t strPtr = top;
    uint64_t argPtr[16], envPtr[16];
    for (int i = (int)envc - 1; i >= 0; i--) {
        uint64_t n = slenk(envv[i]) + 1;
        strPtr -= n;
        copyToUser(pml4, strPtr, (const uint8_t *)envv[i], n, 0);
        envPtr[i] = strPtr;
    }
    for (int i = (int)argc - 1; i >= 0; i--) {
        uint64_t n = slenk(argv[i]) + 1;
        strPtr -= n;
        copyToUser(pml4, strPtr, (const uint8_t *)argv[i], n, 0);
        argPtr[i] = strPtr;
    }
    strPtr -= 16;                                          /* AT_RANDOM: 16 byte */
    uint8_t rnd[16];
    for (int i = 0; i < 16; i++) rnd[i] = (uint8_t)(0xA5 ^ (i * 37));
    copyToUser(pml4, strPtr, rnd, 16, 0);
    uint64_t randomAt = strPtr;
    uint64_t rsp = strPtr & ~0xFULL;                       /* như cavOS: phần dưới bắt đầu ở mốc 16 */

    /* auxv (đẩy ngược: AT_NULL vào trước, nên nằm cuối danh sách) — cùng thứ tự cavOS */
    push64(pml4, &rsp, 0);  push64(pml4, &rsp, 0);              /* AT_NULL */
    push64(pml4, &rsp, randomAt); push64(pml4, &rsp, 25);       /* AT_RANDOM */
    push64(pml4, &rsp, 4096); push64(pml4, &rsp, 6);            /* AT_PAGESZ */
    push64(pml4, &rsp, 0);  push64(pml4, &rsp, 23);             /* AT_SECURE */
    push64(pml4, &rsp, eh->e_phnum); push64(pml4, &rsp, 5);     /* AT_PHNUM */
    push64(pml4, &rsp, eh->e_phentsize); push64(pml4, &rsp, 4); /* AT_PHENT */
    push64(pml4, &rsp, eh->e_entry); push64(pml4, &rsp, 9);     /* AT_ENTRY */
    push64(pml4, &rsp, 0);  push64(pml4, &rsp, 7);              /* AT_BASE (no interpreter) */
    push64(pml4, &rsp, 0);  push64(pml4, &rsp, 8);              /* AT_FLAGS */
    push64(pml4, &rsp, 0);  push64(pml4, &rsp, 16);             /* AT_HWCAP */
    push64(pml4, &rsp, lowest + eh->e_phoff); push64(pml4, &rsp, 3);   /* AT_PHDR */
    push64(pml4, &rsp, 0);                                       /* end of environ */
    for (int i = (int)envc - 1; i >= 0; i--) push64(pml4, &rsp, envPtr[i]);
    push64(pml4, &rsp, 0);                                       /* end of argv */
    for (int i = (int)argc - 1; i >= 0; i--) push64(pml4, &rsp, argPtr[i]);
    push64(pml4, &rsp, argc);
    t->registers.usermode_rsp = rsp;

    serial_puts("[elf] stack: strings + AT_RANDOM from ");
    hx(randomAt);
    serial_puts(" to ");
    hx(top);
    serial_puts(", RSP = ");
    hx(rsp);
    serial_puts(" (RSP % 16 = ");
    serial_putdec(rsp % 16);
    serial_puts("), ");
    serial_putdec((top - rsp) / 8);
    serial_puts(" qwords in all\n");
    serial_puts("[elf]   [RSP]    = ");
    serial_putdec(peek64(pml4, rsp));
    serial_puts(" (argc)\n");
    for (uint32_t i = 0; i < argc; i++) {
        serial_puts("[elf]   [RSP+");
        serial_putdec(8 + 8 * i);
        serial_puts("] = ");
        hx(peek64(pml4, rsp + 8 + 8 * i));
        serial_puts(" (argv[");
        serial_putdec(i);
        serial_puts("])\n");
    }
    return t;
}
