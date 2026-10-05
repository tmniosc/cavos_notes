/* user.S — chương trình ring 3 của Lab 0x0b (Bài 17). Kernel chép byte giữa *_start và *_end
 * vào một trang PTE_USER, nên code chỉ dùng địa chỉ tương đối theo RIP.
 *
 * fpu_A / fpu_B: hai bản giống nhau, khác mẫu (byte 0xAA / 0x55). Mỗi bản:
 *   nạp mẫu 32 byte vào YMM0 (cả nửa thấp XMM0 lẫn nửa cao), nạp một số nguyên vào đỉnh
 *   stack x87 (fild), rồi 40 lần: quay ~200000 vòng (đủ để timer 1 ms cắt và đổi sang task
 *   kia), cất YMM0 và ST(0) ra stack, so với mẫu. Báo bằng syscall 444:
 *   rdi = mẫu, rsi = số lần kiểm, rdx = số lần nửa thấp (XMM0) sai, r10 = nửa cao sai,
 *   r8 = số lần x87 sai. Rồi exit(0).
 * sse_probe: một lệnh SSE (movaps) và một lệnh AVX (vmovdqu) rồi exit(0); dùng để thấy #UD khi
 * chưa bật.
 */
    .section .rodata
    .global user_fpuA_start, user_fpuA_end, user_fpuB_start, user_fpuB_end
    .global user_probe_start, user_probe_end

.macro FPUTEST name, byte, num
user_\name\()_start:
    vmovdqu pat_\name(%rip), %ymm0
    fildq   num_\name(%rip)            /* ST(0) = \num */
    xor     %r12d, %r12d                /* rounds */
    xor     %r13d, %r13d                /* low half bad */
    xor     %r14d, %r14d                /* high half bad */
    xor     %r15d, %r15d                /* x87 bad */
1:  mov     $200000, %ecx
2:  dec     %ecx
    jnz     2b
    sub     $48, %rsp
    vmovdqu %ymm0, (%rsp)
    fistl   32(%rsp)                    /* store ST(0) as a 32-bit int, keep it on the stack */
    mov     (%rsp), %rax
    cmp     pat_\name(%rip), %rax
    jne     3f
    mov     8(%rsp), %rax
    cmp     pat_\name+8(%rip), %rax
    je      4f
3:  inc     %r13d
4:  mov     16(%rsp), %rax
    cmp     pat_\name+16(%rip), %rax
    jne     5f
    mov     24(%rsp), %rax
    cmp     pat_\name+24(%rip), %rax
    je      6f
5:  inc     %r14d
6:  cmpl    $\num, 32(%rsp)
    je      7f
    inc     %r15d
7:  add     $48, %rsp
    inc     %r12d
    cmp     $40, %r12d
    jne     1b

    mov     $\byte, %edi
    mov     %r12, %rsi
    mov     %r13, %rdx
    mov     %r14, %r10
    mov     %r15, %r8
    mov     $444, %eax
    syscall
    mov     $60, %eax
    xor     %edi, %edi
    syscall
    ud2
    .balign 32
pat_\name: .fill 32, 1, \byte
num_\name: .quad \num
user_\name\()_end:
.endm

FPUTEST fpuA, 0xAA, 1111
FPUTEST fpuB, 0x55, 2222

user_probe_start:
    movaps  %xmm0, %xmm1                /* SSE: #UD if CR4.OSFXSR = 0 */
    vmovdqu %ymm0, %ymm1                /* AVX: #UD if CR4.OSXSAVE = 0 or XCR0.AVX = 0 */
    mov     $60, %eax
    xor     %edi, %edi
    syscall
    ud2
user_probe_end:
