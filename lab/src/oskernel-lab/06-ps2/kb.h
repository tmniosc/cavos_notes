/* kb.h — bàn phím PS/2, theo cavOS drivers/kb.c + include/kb.h (Bài 11). */
#pragma once
#include <stdint.h>
#include "input.h"

#define SCANCODE_ENTER 28
#define SCANCODE_BACK  14
#define SCANCODE_SHIFT 42          /* shift trái; cavOS không có shift phải (54) */
#define SCANCODE_CAPS  58
#define SCANCODE_RSHIFT 54         /* lab thêm */

#define CHARACTER_ENTER '\n'
#define CHARACTER_BACK  '\b'

extern DevInputEvent *kbEvent;     /* /dev/input/event0 */

void initiateKb(void);             /* = cavOS initiateKb() */
