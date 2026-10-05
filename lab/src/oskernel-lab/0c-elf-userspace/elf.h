/* elf.h — nạp chương trình ELF64 vào một task ring 3, theo cavOS utilities/elf.c (elfExecute)
 * + multitasking/stack.c (stackGenerateUser). Bài 18. */
#pragma once
#include <stdint.h>
#include "task.h"

typedef struct {
    uint8_t  e_ident[16];      /* 7F 'E' 'L' 'F', class, data, version, ... */
    uint16_t e_type;           /* 2 = ET_EXEC, 3 = ET_DYN */
    uint16_t e_machine;        /* 0x3E = x86-64 */
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;           /* 1 = PT_LOAD, 3 = PT_INTERP */
    uint32_t p_flags;          /* 1 X, 2 W, 4 R */
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_LOAD 1

/* = cavOS elfExecute(filepath, argc, argv, envc, envv, startup = false): task ở CREATED,
 * người gọi taskCreateFinish(). Trả 0 nếu không mở được hoặc file không hợp lệ. */
Task *elfExecute(const char *path, uint32_t argc, const char **argv, uint32_t envc, const char **envv);
