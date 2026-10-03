/* mouse.c — chuột PS/2, theo cavOS drivers/mouse.c (Bài 11).
 *
 * Giống cavOS: initiateMouse() = 0xA8 (bật cổng 2) -> đọc byte cấu hình (0x20), bật bit 1
 * (IRQ12), ghi lại (0x60) -> gửi chuột 0xF6 (mặc định) và 0xF4 (bật gửi dữ liệu), mỗi lệnh
 * đọc 1 ACK -> ioApicRedirect(12) -> mouseIrq. mouseIrq gom 3 byte thành 1 gói, đổi dấu
 * 9 bit, sinh EV_KEY BTN_LEFT/BTN_RIGHT khi nút đổi và EV_REL REL_X/REL_Y (Y đảo dấu).
 * Giữ cả giá trị đầu clickedRight = true của cavOS (gói đầu tiên sinh 1 sự kiện
 * "nhả nút phải" thừa; log sẽ thấy).
 * Khác cavOS:
 *  - cavOS assert(byte 0 có bit 3) -> lệch nhịp là panic. Lab bỏ byte và đồng bộ lại.
 *  - cavOS "if (x && sign) x -= 0x100" (x = 0 có bit dấu thành 0 thay vì -256). Lab bỏ "x &&".
 *  - Không kẹp toạ độ theo framebuffer (gx/gy): lab không có framebuffer, và cavOS cũng
 *    chỉ gửi REL_X/REL_Y (ABS đã comment).
 *  - In giá trị byte cấu hình và ACK để quan sát.
 */
#include "mouse.h"
#include "apic.h"
#include "io.h"
#include "isr.h"
#include "ps2.h"
#include "serial.h"
#include "timer.h"

DevInputEvent *mouseEvent;
volatile uint32_t mouseResyncs;

/* = cavOS mouseWait(a_type): 0 = chờ có byte để đọc, 1 = chờ được phép ghi. Hết giờ thì
 * cứ đi tiếp (không báo lỗi), như cavOS. */
static void mouseWait(uint8_t a_type) {
    uint32_t timeout = MOUSE_TIMEOUT;
    if (!a_type) {
        while (--timeout)
            if (inb(MOUSE_STATUS) & MOUSE_BBIT)
                break;
    } else {
        while (--timeout)
            if (!(inb(MOUSE_STATUS) & MOUSE_ABIT))
                break;
    }
}

static void mouseWrite(uint8_t write) {         /* 0xD4 rồi byte: gửi cho chuột */
    mouseWait(1);
    outb(MOUSE_STATUS, MOUSE_WRITE);
    mouseWait(1);
    outb(MOUSE_PORT, write);
}

static uint8_t mouseRead(void) {
    mouseWait(0);
    return inb(MOUSE_PORT);
}

/* ---------------------------------------------------------------- IRQ12 */
static int     mouseCycle;
static uint8_t mouse1, mouse2;
static uint8_t st1, st2;
static int     clickedLeft = 0;
static int     clickedRight = 1;                /* cavOS: bool clickedRight = true; */

static void mouseIrq(AsmPassedInterrupt *regs) {
    (void)regs;
    uint8_t st = inb(MOUSE_STATUS);
    uint8_t byte = mouseRead();

    if (mouseCycle == 0) {
        if (!(byte & 0x08)) {                   /* byte đầu gói luôn có bit 3 = 1 */
            ps2_trace t = {0};
            t.kind = TRACE_MOUSE_RESYNC;
            t.status[0] = st;
            t.byte[0] = byte;
            ring_put(&trace_q, &t);
            mouseResyncs++;
            return;                             /* cavOS: assert -> panic */
        }
        mouse1 = byte;
        st1 = st;
    } else if (mouseCycle == 1) {
        mouse2 = byte;
        st2 = st;
    } else {
        int x = mouse2;
        int y = byte;
        if (mouse1 & (1 << 4))                  /* bit 4 = dấu X (bit thứ 9) */
            x -= 0x100;
        if (mouse1 & (1 << 5))                  /* bit 5 = dấu Y */
            y -= 0x100;

        int click = mouse1 & (1 << 0);
        int rclick = (mouse1 >> 1) & 1;

        if (clickedLeft && !click)
            inputGenerateEvent(mouseEvent, EV_KEY, BTN_LEFT, 0);
        if (!clickedLeft && click)
            inputGenerateEvent(mouseEvent, EV_KEY, BTN_LEFT, 1);
        if (clickedRight && !rclick)
            inputGenerateEvent(mouseEvent, EV_KEY, BTN_RIGHT, 0);
        if (!clickedRight && rclick)
            inputGenerateEvent(mouseEvent, EV_KEY, BTN_RIGHT, 1);
        clickedRight = rclick;
        clickedLeft = click;

        inputGenerateEvent(mouseEvent, EV_REL, REL_X, x);
        inputGenerateEvent(mouseEvent, EV_REL, REL_Y, -y);   /* PS/2: +y = lên; evdev: +y = xuống */
        inputGenerateEvent(mouseEvent, EV_SYN, SYN_REPORT, 0);

        ps2_trace t = {0};
        t.kind = TRACE_MOUSE;
        t.status[0] = st1; t.status[1] = st2; t.status[2] = st;
        t.byte[0] = mouse1; t.byte[1] = mouse2; t.byte[2] = byte;
        t.dx = (int16_t)x;
        t.dy = (int16_t)y;
        t.buttons = mouse1 & 7;
        ring_put(&trace_q, &t);
    }

    mouseCycle++;
    if (mouseCycle > 2)
        mouseCycle = 0;
}

/* --------------------------------------------------------------- init */
void initiateMouse(void) {
    mouseEvent = devInputEventSetup("PS/2 Mouse");

    /* enable the auxiliary mouse */
    mouseWait(1);
    outb(MOUSE_STATUS, PS2_CMD_ENABLE_PORT2);           /* 0xA8 */

    /* enable interrupts: đọc byte cấu hình, bật bit 1 (IRQ12), ghi lại */
    mouseWait(1);
    outb(MOUSE_STATUS, PS2_CMD_READ_CONFIG);            /* 0x20 */
    sleep(100);
    mouseWait(0);
    sleep(100);
    uint8_t before = inb(MOUSE_PORT);
    uint8_t status = before | 2;
    mouseWait(1);
    outb(MOUSE_STATUS, PS2_CMD_WRITE_CONFIG);           /* 0x60 */
    mouseWait(1);
    outb(MOUSE_PORT, status);
    ps2_print_config("[mouse] config read after 0xA8 = ", before);
    ps2_print_config("[mouse] config written (|= 2)  = ", status);

    /* default settings */
    mouseWrite(0xF6);
    uint8_t ack1 = mouseRead();
    /* enable device (data reporting) */
    mouseWrite(0xF4);
    uint8_t ack2 = mouseRead();
    serial_puts("[mouse] 0xD4 0xF6 (set defaults) -> ");
    serial_puthex_short(ack1);
    serial_puts(", 0xD4 0xF4 (enable reporting) -> ");
    serial_puthex_short(ack2);
    serial_puts(ack1 == 0xFA && ack2 == 0xFA ? "  (0xFA = ACK)\n" : "\n");

    /* irq handler */
    uint8_t targIrq = ioApicRedirect(12, 0);
    register_irq_handler(targIrq, mouseIrq);
}
