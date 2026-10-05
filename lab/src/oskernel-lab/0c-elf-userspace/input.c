/* input.c — ring buffer + /dev/input/eventN giả, theo cavOS dev_event.c (Bài 11). */
#include "input.h"
#include "timer.h"

void ring_init(ring_t *q, void *storage, uint32_t cap, uint32_t esz) {
    q->w = q->r = 0;
    q->cap = cap;
    q->esz = esz;
    q->buf = storage;
    q->dropped = 0;
}

static void copy(uint8_t *d, const uint8_t *s, uint32_t n) {
    while (n--)
        *d++ = *s++;
}

int ring_put(ring_t *q, const void *rec) {
    uint32_t w = q->w, next = (w + 1) % q->cap;
    if (next == q->r) {                       /* đầy: chừa 1 ô để phân biệt đầy/rỗng */
        q->dropped++;
        return 0;
    }
    copy(q->buf + w * q->esz, rec, q->esz);
    __asm__ volatile("" ::: "memory");        /* ghi dữ liệu xong mới dời chỉ số */
    q->w = next;
    return 1;
}

int ring_get(ring_t *q, void *rec) {
    uint32_t r = q->r;
    if (r == q->w)
        return 0;
    copy(rec, q->buf + r * q->esz, q->esz);
    __asm__ volatile("" ::: "memory");
    q->r = (r + 1) % q->cap;
    return 1;
}

/* ------------------------------------------------------------ evdev */
#define MAX_EVENTS 8                           /* như cavOS include/dev.h */
static DevInputEvent devInputEvents[MAX_EVENTS];
static int lastInputEvent;

DevInputEvent *devInputEventSetup(const char *devname) {
    if (lastInputEvent >= MAX_EVENTS)
        return 0;
    DevInputEvent *item = &devInputEvents[lastInputEvent];
    item->devname = devname;
    item->num = lastInputEvent;               /* cavOS: snprintf(name, "event%d", ...) */
    ring_init(&item->q, item->storage, 64, sizeof(input_event));
    lastInputEvent++;
    return item;
}

void devInputEventOpen(DevInputEvent *item) { item->timesOpened++; }

void inputGenerateEvent(DevInputEvent *item, uint16_t type, uint16_t code, int32_t value) {
    if (item->timesOpened == 0)               /* không ai mở file -> bỏ, như cavOS */
        return;
    input_event ev = {timerTicks, type, code, value};
    ring_put(&item->q, &ev);                  /* cavOS: assert(CircularIntWrite(...) == size) */
}

/* -------------------------------------------------------- trace + tty */
static ps2_trace trace_storage[128];
static tty_char  tty_storage[256];
ring_t trace_q, tty_q;

void input_init(void) {
    ring_init(&trace_q, trace_storage, 128, sizeof(ps2_trace));
    ring_init(&tty_q, tty_storage, 256, sizeof(tty_char));
}
