/* mouse.h — chuột PS/2, theo cavOS drivers/mouse.c + include/mouse.h (Bài 11). */
#pragma once
#include <stdint.h>
#include "input.h"

#define MOUSE_PORT    0x60
#define MOUSE_STATUS  0x64
#define MOUSE_ABIT    0x02         /* IBF: chờ bit này về 0 trước khi ghi */
#define MOUSE_BBIT    0x01         /* OBF: chờ bit này lên 1 trước khi đọc */
#define MOUSE_WRITE   0xD4         /* "byte kế tiếp gửi cho chuột" */
#define MOUSE_TIMEOUT 100000

extern DevInputEvent *mouseEvent;  /* /dev/input/event1 */
extern volatile uint32_t mouseResyncs;

void initiateMouse(void);          /* = cavOS initiateMouse() */
