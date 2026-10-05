/* hello.c — chương trình ring 3 của Lab 0x0c, build thành ELF tĩnh, đặt ở /bin/hello trên ext2.
 * Kernel (elf.c) nạp nó như elfExecute() của cavOS. Chương trình in lại mọi thứ kernel đưa cho
 * nó trên stack lúc bắt đầu: argc, argv, envp, auxv, và RSP lúc vào.
 */
#include "ulib.h"

static const char *auxName(u64 t) {
    switch (t) {
    case 3: return "AT_PHDR";   case 4: return "AT_PHENT"; case 5: return "AT_PHNUM";
    case 6: return "AT_PAGESZ"; case 7: return "AT_BASE";  case 8: return "AT_FLAGS";
    case 9: return "AT_ENTRY";  case 16: return "AT_HWCAP"; case 23: return "AT_SECURE";
    case 25: return "AT_RANDOM";
    default: return "AT_?";
    }
}

static volatile char bss[4096];        /* .bss: phải đọc ra toàn 0 */
static volatile int dataVal = 1234;    /* .data: giá trị từ file */

void cmain(u64 *sp, u64 entryRsp) {
    Line l = {0};
    ls_(&l, "hello from /bin/hello, an ELF file loaded from ext2 into ring 3"); lend_(&l);
    ls_(&l, "entry RSP = "); lx_(&l, entryRsp);
    ls_(&l, " (RSP % 16 = "); ld_(&l, entryRsp % 16); ls_(&l, ")"); lend_(&l);

    i64 argc = (i64)sp[0];
    char **argv = (char **)(sp + 1);
    char **envp = argv + argc + 1;
    ls_(&l, "argc = "); ld_(&l, argc); lend_(&l);
    for (i64 i = 0; i < argc; i++) {
        ls_(&l, "argv["); ld_(&l, i); ls_(&l, "] = \""); ls_(&l, argv[i]);
        ls_(&l, "\" at "); lx_(&l, (u64)argv[i]); lend_(&l);
    }
    char **e = envp;
    for (; *e; e++) {
        ls_(&l, "envp["); ld_(&l, (u64)(e - envp)); ls_(&l, "] = \""); ls_(&l, *e); ls_(&l, "\""); lend_(&l);
    }
    u64 *aux = (u64 *)(e + 1);
    for (; aux[0]; aux += 2) {
        ls_(&l, "auxv: "); ls_(&l, auxName(aux[0])); ls_(&l, " ("); ld_(&l, aux[0]);
        ls_(&l, ") = "); lx_(&l, aux[1]); lend_(&l);
    }

    u64 nonzero = 0;
    for (int i = 0; i < 4096; i++) nonzero += bss[i] != 0;
    ls_(&l, ".data dataVal = "); ld_(&l, (u64)dataVal);
    ls_(&l, ", .bss 4096 bytes, non-zero: "); ld_(&l, nonzero); lend_(&l);

    double x = (double)argc * 1.5 + 0.25;  /* SSE (mulsd/addsd): Bài 17 phải bật xong */
    ls_(&l, "double argc * 1.5 + 0.25 = "); ld_(&l, (u64)(x * 100));
    ls_(&l, " / 100"); lend_(&l);
    exit_((int)argc);
}

/* _start: RSP trỏ vào argc. Lưu RSP lúc vào, căn 16 rồi gọi C (như crt1 của musl). */
__asm__(".global _start\n"
        "_start:\n"
        "  mov %rsp, %rdi\n"
        "  mov %rsp, %rsi\n"
        "  and $-16, %rsp\n"
        "  call cmain\n"
        "  ud2\n");
