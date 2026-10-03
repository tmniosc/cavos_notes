/* input.h — hàng đợi sự kiện input, theo cavOS filesystems/dev/dev_event.c (Bài 11).
 *
 * cavOS: mỗi thiết bị gọi devInputEventSetup("PS/2 Keyboard") -> được 1 file
 * /dev/input/eventN (kb = event0, mouse = event1 vì initiateKb chạy trước). IRQ gọi
 * inputGenerateEvent() ghi struct input_event (kiểu Linux evdev) vào CircularInt của
 * thiết bị; chương trình userspace open() + read() file đó để lấy sự kiện.
 * Lab chưa có VFS (/dev/ là Bài 15) nên giữ đúng phần lõi: một ring buffer cho mỗi
 * thiết bị, IRQ ghi, vòng lặp chính đọc.
 * Thêm của lab: ring "trace" ghi từng byte đọc từ cổng 0x60 kèm thanh ghi trạng thái,
 * để thấy phần cứng gửi gì (cavOS không có, chỉ có debugf đã comment).
 */
#pragma once
#include <stdint.h>

/* Mã Linux evdev (include/uapi/linux/input-event-codes.h), cavOS dùng y như vậy. */
#define EV_SYN     0x00
#define EV_KEY     0x01
#define EV_REL     0x02
#define SYN_REPORT 0
#define REL_X      0x00
#define REL_Y      0x01
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111

/* ---------------------------------------------------------------- ring */
/* Một producer (IRQ) + một consumer (vòng lặp chính): chỉ cần 2 chỉ số volatile.
 * Đầy thì bỏ sự kiện và đếm (cavOS: assert -> panic, xem Bài 11 Đọc thêm). */
typedef struct {
    volatile uint32_t w, r;
    uint32_t cap, esz;
    uint8_t *buf;
    volatile uint32_t dropped;
} ring_t;

void ring_init(ring_t *q, void *storage, uint32_t cap, uint32_t esz);
int  ring_put(ring_t *q, const void *rec);     /* 1 = ok, 0 = đầy */
int  ring_get(ring_t *q, void *rec);           /* 1 = có, 0 = rỗng */

/* --------------------------------------------------------- evdev giả */
typedef struct {                   /* = struct input_event của cavOS/Linux, thời gian tính bằng tick */
    uint64_t ticks;
    uint16_t type, code;
    int32_t  value;
} input_event;

typedef struct {                   /* = DevInputEvent (bỏ ioctl/eventBit) */
    const char *devname;
    int         num;               /* N trong /dev/input/eventN */
    int         timesOpened;       /* 0 -> inputGenerateEvent bỏ qua, như cavOS */
    ring_t      q;
    input_event storage[64];
} DevInputEvent;

DevInputEvent *devInputEventSetup(const char *devname);
void inputGenerateEvent(DevInputEvent *item, uint16_t type, uint16_t code, int32_t value);
void devInputEventOpen(DevInputEvent *item);   /* như open("/dev/input/eventN") */

/* ------------------------------------------------------- trace của lab */
enum { TRACE_KB = 1, TRACE_MOUSE = 2, TRACE_MOUSE_RESYNC = 3 };

typedef struct {
    uint8_t kind;
    uint8_t status[3];             /* cổng 0x64 ngay trước khi đọc 0x60 */
    uint8_t byte[3];               /* KB: byte[0]; MOUSE: 3 byte của gói */
    char    ch;                    /* KB: ký tự theo luật của lab (đã sửa) */
    char    ch_cavos;              /* KB: ký tự theo đúng luật cavOS handleKbEvent() */
    int16_t dx, dy;                /* MOUSE: đã đổi dấu 9 bit */
    uint8_t buttons;               /* MOUSE: bit 0 L, 1 R, 2 M */
} ps2_trace;

extern ring_t trace_q;
extern ring_t tty_q;               /* ký tự cho "task đang read()" (= kbBuff của cavOS) */

void input_init(void);             /* = fsMount("/dev/", CONNECTOR_DEV, ...) của cavOS, bản rút gọn */
