/* kernel_helper.h — = cavOS include/kernel_helper.h + entry/kernel_helper.c (Bài 12).
 * initiateKernelThreads() tạo thread "helper": vòng lặp helperNet + helperReaper +
 * helperVolatilePoll + handControl(). Lab giữ helperReaper (dọn task đã chết) và từ
 * Lab 0x08 thêm helperNet (đưa frame từ netQueue lên stack). */
#pragma once
#include <stdint.h>

typedef struct {                   /* lab: lưu số liệu của task đã dọn để in bảng cuối */
    uint64_t    id;
    const char *name;
    uint64_t    cpuTicks, switchesIn, wakeups, reapedAt;
    int         frames, tables;
} ReapedInfo;

extern ReapedInfo reaped[16];
extern int        reapedCount;

void initiateKernelThreads(void);  /* = cavOS initiateKernelThreads() */
