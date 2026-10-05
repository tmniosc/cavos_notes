/* sse.h — bật x87/SSE/AVX theo cavOS cpu/system.c initiateSSE() (Bài 17). */
#pragma once
#include <stdint.h>

void initiateSSE(void);
void sseDump(const char *when);
/* Lab: cỡ vùng lưu trạng thái FPU khi đổi task. fxsave: 512 byte (x87 + SSE).
 * xsave với XCR0 = 7: cần thêm 256 byte nửa cao YMM (CPUID 0xD) -> lab giữ 1024 byte. */
#define FPU_AREA_SIZE 1024
extern int sseAvxEnabled;
extern volatile int sseReady;         /* lab: initiateSSE() đã chạy xong */
