/* Lab 0x06 — PS/2 Keyboard & Mouse (Bài 11).
 * Thứ tự giống cavOS _start():
 *   ... initiateACPI -> initiateISR (APIC + sti) -> initiateApicTimer ("mouse needs a timer")
 *   -> fsMount("/dev/", CONNECTOR_DEV) ("mouse & kb need it") -> initiateKb -> initiateMouse.
 * Lab: /dev/ thu gọn thành input_init() (ring buffer), thêm ps2_probe() trước initiateKb để
 * xem controller 8042 do firmware để lại thế nào.
 * Demo: vòng lặp chính = "chương trình đọc /dev/input/event0, event1 và stdin": in từng byte
 * IRQ đọc được, từng sự kiện evdev, và từng dòng read() trả về. Esc thì dừng.
 * F2 bật/tắt bit translate của controller để thấy scancode set 2 thô.
 * Input thật đến từ QEMU monitor (sendkey, mouse_move, mouse_button): scripts/ps2_input.py.
 */
#include "../limine.h"
#include "boot.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "acpi.h"
#include "apic.h"
#include "timer.h"
#include "io.h"
#include "ps2.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "serial.h"

LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static void putch_visible(char c) {
    if (c == '\n')      serial_puts("'\\n'");
    else if (c == '\b') serial_puts("'\\b'");
    else if (c == '\t') serial_puts("'\\t'");
    else if (c == 27)   serial_puts("ESC");
    else if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7f) {
        serial_puts("ctrl ");
        serial_puthex_short((uint8_t)c);
    } else {
        serial_putc('\'');
        serial_putc(c);
        serial_putc('\'');
    }
}

static const char *key_name(uint8_t key, int ext) {
    if (ext) {
        switch (key) {
        case 0x48: return "Up";
        case 0x50: return "Down";
        case 0x4B: return "Left";
        case 0x4D: return "Right";
        case 0x1C: return "KP-Enter";
        default:   return "extended";
        }
    }
    switch (key) {
    case 1:    return "Esc";
    case 14:   return "Backspace";
    case 28:   return "Enter";
    case 42:   return "L-Shift";
    case 54:   return "R-Shift";
    case 57:   return "Space";
    case 58:   return "CapsLock";
    case 0x3C: return "F2";
    case 0x41: return "F7";
    case 0x48: return "KP8";
    case 0x50: return "KP2";
    default:   return 0;
    }
}

static void putsigned(int v) {
    if (v < 0) { serial_putc('-'); serial_putdec((uint64_t)(-v)); }
    else       { serial_putc('+'); serial_putdec((uint64_t)v); }
}

/* --------------------------------------------- trạng thái của vòng lặp chính */
static uint8_t cfg_now;          /* byte cấu hình hiện tại (không đọc lại 0x20 khi đang chạy) */
static int     translate_on = 1;
static int     prev_e0, prev_f0;
static int     esc_seen;
static char    line[128];
static int     line_len;
static int     lines_read;

static void set_translate(int on) {
    cfg_now = on ? (cfg_now | PS2_CFG_TRANSLATE) : (cfg_now & ~PS2_CFG_TRANSLATE);
    /* Chỉ GHI (0x60 + byte): không sinh byte trả lời nên IRQ1 không bị lẫn. */
    __asm__ volatile("cli");
    while (inb(PS2_STATUS) & PS2_ST_IBF) ;
    outb(PS2_COMMAND, PS2_CMD_WRITE_CONFIG);
    while (inb(PS2_STATUS) & PS2_ST_IBF) ;
    outb(PS2_DATA, cfg_now);
    __asm__ volatile("sti");
    translate_on = on;
    serial_puts("[8042] F2 -> translate=");
    serial_putdec(on);
    serial_puts(", config byte now ");
    serial_puthex_short(cfg_now);
    serial_puts(on ? "  (back to set 1 codes)\n" : "  (keyboard bytes now arrive as raw set 2)\n");
}

static void print_kb(const ps2_trace *t) {
    uint8_t b = t->byte[0];
    serial_puts("[irq1]  0x64=");
    serial_puthex_short(t->status[0]);
    serial_puts(" 0x60=");
    serial_puthex_short(b);
    if (!translate_on) {
        serial_puts("  raw set 2");
        if (b == 0xF0) serial_puts(" (break prefix)");
        if (b == 0xE0) serial_puts(" (extended prefix)");
    } else if (b == 0xE0) {
        serial_puts("  prefix E0 (extended key follows)");
    } else {
        const char *n = key_name(b & 0x7F, prev_e0);
        serial_puts(b & 0x80 ? "  break " : "  make  ");
        serial_puts(n ? n : "key");
        if (t->ch) {
            serial_puts(" -> ");
            putch_visible(t->ch);
        }
    }
    if (t->ch_cavos != t->ch) {
        serial_puts("   [cavOS rule gives ");
        if (t->ch_cavos) putch_visible(t->ch_cavos);
        else serial_puts("nothing");
        serial_puts("]");
    }
    serial_putc('\n');

    /* F2 bật/tắt dịch. Set 1 (đã dịch): F2 make = 0x3C. Set 2 thô: F2 make = 0x06,
     * break = F0 06 -> phải bỏ qua 0x06 đi sau 0xF0. */
    if (translate_on && b == 0x3C && !prev_e0)
        set_translate(0);
    else if (!translate_on && b == 0x06 && !prev_f0)
        set_translate(1);
    prev_e0 = (b == 0xE0);
    prev_f0 = (b == 0xF0);
}

static void print_mouse(const ps2_trace *t) {
    if (t->kind == TRACE_MOUSE_RESYNC) {
        serial_puts("[irq12] 0x60=");
        serial_puthex_short(t->byte[0]);
        serial_puts(" has bit 3 = 0 -> not a packet start, dropped (cavOS: assert -> panic)\n");
        return;
    }
    serial_puts("[irq12] bytes ");
    for (int i = 0; i < 3; i++) { serial_puthex_short(t->byte[i]); serial_putc(' '); }
    serial_puts("(0x64 ");
    for (int i = 0; i < 3; i++) {
        serial_puthex_short(t->status[i]);
        serial_puts(i < 2 ? " " : ")");
    }
    serial_puts(" -> dx=");
    putsigned(t->dx);
    serial_puts(" dy=");
    putsigned(t->dy);
    serial_puts(" L=");
    serial_putdec(t->buttons & 1);
    serial_puts(" R=");
    serial_putdec((t->buttons >> 1) & 1);
    serial_puts(" M=");
    serial_putdec((t->buttons >> 2) & 1);
    if (t->byte[0] & 0xC0)
        serial_puts(" OVERFLOW");
    serial_putc('\n');
}

static const char *ev_type(uint16_t t) {
    return t == EV_SYN ? "EV_SYN" : t == EV_KEY ? "EV_KEY" : t == EV_REL ? "EV_REL" : "EV_?";
}

static void drain_device(DevInputEvent *d) {
    input_event ev;
    int open_line = 0;
    while (ring_get(&d->q, &ev)) {
        if (!open_line) {
            serial_puts("        /dev/input/event");
            serial_putdec(d->num);
            serial_puts(":");
            open_line = 1;
        }
        if (ev.type == EV_SYN) {
            serial_puts(" SYN\n");
            open_line = 0;
            continue;
        }
        serial_putc(' ');
        serial_puts(ev_type(ev.type));
        serial_putc(' ');
        if (ev.type == EV_REL)
            serial_puts(ev.code == REL_X ? "REL_X" : "REL_Y");
        else if (ev.code == BTN_LEFT)
            serial_puts("BTN_LEFT");
        else if (ev.code == BTN_RIGHT)
            serial_puts("BTN_RIGHT");
        else {
            serial_puts("code ");
            serial_putdec(ev.code);
        }
        serial_putc('=');
        if (ev.type == EV_REL) putsigned(ev.value);
        else serial_putdec((uint64_t)ev.value);
        serial_putc(',');
        if (ev.type == EV_KEY && ev.code == 1 && ev.value == 1)
            esc_seen = 1;                         /* KEY_ESC nhấn -> kết thúc demo */
    }
    if (open_line)
        serial_putc('\n');
}

/* "Task đang read() stdin" ở chế độ ICANON: gom tới Enter, Backspace xoá 1 ký tự.
 * cavOS làm việc này ngay trong kbIrq (kbChar / kbFinaliseStream) rồi đánh thức task. */
static void drain_tty(void) {
    char c;
    while (ring_get(&tty_q, &c)) {
        if (c == '\n') {
            serial_puts("[tty]   read() returns \"");
            for (int i = 0; i < line_len; i++)
                serial_putc(line[i]);
            serial_puts("\\n\" (");
            serial_putdec(line_len + 1);
            serial_puts(" bytes)\n");
            line_len = 0;
            lines_read++;
        } else if (c == '\b') {
            if (line_len > 0)
                line_len--;
        } else if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7f && line_len < 127) {
            line[line_len++] = c;
        }
    }
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    boot_init();

    serial_puts("=== Lab 0x06 - PS/2 Keyboard & Mouse ===\n\n");

    pmm_init();                                   /* Bài 5 */
    paging_init();                                /* Bài 6 */
    gdt_init();                                   /* Bài 7 */
    acpi_init();                                  /* Bài 8: chỉ cần MADT */
    isr_init();                                   /* Bài 9 + 10: IDT, APIC, sti */
    serial_puts("[boot] pmm, paging, GDT/TSS, ACPI, IDT, APIC ready; interrupts on\n");
    timer_init();                                 /* Bài 10, = initiateApicTimer() */
    serial_putc('\n');

    input_init();                                 /* = fsMount("/dev/", CONNECTOR_DEV, 0, 0) */
    ps2_probe();                                  /* lab: cavOS không có bước này */
    cfg_now = ps2_read_config();
    serial_putc('\n');

    initiateKb();                                 /* = initiateKb() */
    initiateMouse();                              /* = initiateMouse() */
    /* 0xA8 vừa xoá bit 5 (bật clock cổng 2), initiateMouse vừa bật bit 1 (IRQ12). */
    cfg_now = (cfg_now & ~PS2_CFG_CLK2_OFF) | PS2_CFG_IRQ12;

    serial_puts("[ioapic] pins 1 and 12 after initiateKb/initiateMouse:\n");
    ioapic_dump_entries(1, 1);
    ioapic_dump_entries(12, 1);

    /* "userspace" mở /dev/input/event0, event1: từ giờ inputGenerateEvent mới ghi. */
    devInputEventOpen(kbEvent);
    devInputEventOpen(mouseEvent);
    serial_puts("[dev] opened /dev/input/event0 (");
    serial_puts(kbEvent->devname);
    serial_puts(") and /dev/input/event1 (");
    serial_puts(mouseEvent->devname);
    serial_puts(")\n");

    /* Những gì IRQ đã ghi TRƯỚC khi mở thiết bị (vd trong lúc initiateMouse). */
    ps2_trace t;
    while (ring_get(&trace_q, &t)) {
        serial_puts("[early] ");
        if (t.kind == TRACE_KB) print_kb(&t);
        else print_mouse(&t);
    }
    serial_puts("[kernel] ready for input (Esc to stop)\n");

    while (!esc_seen) {
        __asm__ volatile("cli");
        if (trace_q.r == trace_q.w && kbEvent->q.r == kbEvent->q.w &&
            mouseEvent->q.r == mouseEvent->q.w && tty_q.r == tty_q.w)
            __asm__ volatile("sti; hlt");         /* sti;hlt liền nhau: không lỡ ngắt */
        else
            __asm__ volatile("sti");

        while (ring_get(&trace_q, &t)) {
            if (t.kind == TRACE_KB) print_kb(&t);
            else print_mouse(&t);
        }
        drain_device(kbEvent);
        drain_device(mouseEvent);
        drain_tty();
    }

    serial_puts("[kernel] Esc pressed. lines read=");
    serial_putdec(lines_read);
    serial_puts(" dropped: trace=");
    serial_putdec(trace_q.dropped);
    serial_puts(" event0=");
    serial_putdec(kbEvent->q.dropped);
    serial_puts(" event1=");
    serial_putdec(mouseEvent->q.dropped);
    serial_puts(" mouse resyncs=");
    serial_putdec(mouseResyncs);
    serial_puts("\n[kernel] done, cli; hlt\n");
    halt();
}
