/* ulib.h — "libc" tí hon cho chương trình ring 3 của Lab 0x0c: syscall kiểu Linux x86-64
 * (số trong rax, tham số rdi rsi rdx r10 r8 r9, kết quả trong rax) và vài hàm in. */
#pragma once
typedef unsigned long u64;
typedef long i64;

static inline i64 sys3(i64 n, i64 a, i64 b, i64 c) {
    i64 r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}

static inline u64 slen(const char *s) { u64 n = 0; while (s[n]) n++; return n; }
static inline void puts_(const char *s) { sys3(1, 1, (i64)s, (i64)slen(s)); }
static inline __attribute__((noreturn)) void exit_(int code) {
    sys3(60, code, 0, 0);
    for (;;) { }
}

/* một dòng gom trong buffer rồi in một lần (mỗi write là một dòng log) */
typedef struct { char b[160]; int n; } Line;
static inline void ls_(Line *l, const char *s) { while (*s && l->n < 158) l->b[l->n++] = *s++; }
static inline void ld_(Line *l, u64 v) {
    char t[24]; int i = 0;
    do { t[i++] = '0' + v % 10; v /= 10; } while (v);
    while (i && l->n < 158) l->b[l->n++] = t[--i];
}
static inline void lx_(Line *l, u64 v) {
    ls_(l, "0x");
    char t[17]; int i = 0;
    do { t[i++] = "0123456789abcdef"[v & 15]; v >>= 4; } while (v);
    while (i && l->n < 158) l->b[l->n++] = t[--i];
}
static inline void lend_(Line *l) { l->b[l->n++] = '\n'; l->b[l->n] = 0; puts_(l->b); l->n = 0; }
