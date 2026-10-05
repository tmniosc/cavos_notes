/* schedule.h — = cavOS include/schedule.h */
#pragma once
#include <stdint.h>

void schedule(uint64_t rsp);       /* rsp = con trỏ tới khung AsmPassedInterrupt; không return */
