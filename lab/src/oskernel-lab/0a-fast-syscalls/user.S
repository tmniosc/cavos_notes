/* user.S — ba chương trình ring 3 nhỏ (Lab 0x0a). Kernel CHÉP các byte giữa *_start và *_end
 * vào một trang map PTE_USER rồi iretq tới đó, nên code chỉ dùng địa chỉ tương đối theo RIP.
 * Số syscall như Linux/cavOS: 1 write, 39 getpid, 60 exit; 440..443 là syscall báo cáo của lab.
 * (Nằm trong .rodata: kernel không bao giờ chạy những byte này ở chỗ của chúng.)
 */
    .section .rodata
    .global user_main_start, user_main_end, user_gs0_start, user_gs0_end
    .global user_gs1_start, user_gs1_end, user_badret_start, user_badret_end

/* ---------------------------------------------------------------- main */
user_main_start:
    mov  $1, %eax                     /* write(1, msg, len) */
    mov  $1, %edi
    lea  msg(%rip), %rsi
    mov  $msg_end - msg, %edx
    syscall

    mov  $39, %eax                    /* getpid() */
    syscall

    mov  %rsp, %rdi                   /* 440: what the kernel sees */
    lea  1f(%rip), %rsi               /* the address right after the syscall */
    pushfq
    pop  %rdx
    mov  $440, %eax
    syscall
1:  mov  %cs, %rdi                    /* 441: what ring 3 sees after sysret */
    mov  %ss, %rsi
    mov  %rcx, %rdx
    mov  %r11, %r10
    mov  $441, %eax
    syscall

    rdtsc                             /* 10000 x getpid via SYSCALL */
    shl  $32, %rdx
    or   %rdx, %rax
    mov  %rax, %r12
    mov  $10000, %r14d
2:  mov  $39, %eax
    syscall
    dec  %r14d
    jnz  2b
    rdtsc
    shl  $32, %rdx
    or   %rdx, %rax
    sub  %r12, %rax
    mov  %rax, %r13                   /* cycles, SYSCALL */

    rdtsc                             /* 10000 x getpid via int 0x80 */
    shl  $32, %rdx
    or   %rdx, %rax
    mov  %rax, %r12
    mov  $10000, %r14d
3:  mov  $39, %eax
    int  $0x80
    dec  %r14d
    jnz  3b
    rdtsc
    shl  $32, %rdx
    or   %rdx, %rax
    sub  %r12, %rax

    mov  %r13, %rdi                   /* 442: report */
    mov  %rax, %rsi
    mov  $10000, %edx
    mov  $442, %eax
    syscall

    mov  $123, %eax                   /* a number with no handler */
    syscall

    mov  $60, %eax                    /* exit(7) */
    mov  $7, %edi
    syscall
    ud2
msg:
    .ascii "hello from ring 3 (CPL 3) via SYSCALL\n"
msg_end:
user_main_end:

/* ---------------------------------------------------------------- GS before any syscall */
user_gs0_start:
    mov  %gs:0, %rax                  /* GS base = this task's gsbase (0) */
    ud2
user_gs0_end:

/* ---------------------------------------------------------------- GS after a syscall */
user_gs1_start:
    mov  $39, %eax
    syscall                           /* swapgs on entry, none on exit */
    mov  %gs:0, %rax                  /* GS base is now &threadInfo */
    ud2
user_gs1_end:

/* ---------------------------------------------------------------- non-canonical RCX */
user_badret_start:
    mov  $0x0000800000000000, %rdi    /* first non-canonical address above user space */
    mov  $443, %eax
    syscall
    ud2
user_badret_end:
