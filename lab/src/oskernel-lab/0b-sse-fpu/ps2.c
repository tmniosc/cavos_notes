/* ps2.c — phần quan sát controller 8042 của lab (Bài 11). cavOS KHÔNG có các bước này:
 * nó tin firmware đã để controller ở trạng thái dùng được (IRQ1 bật, dịch set 2 -> set 1).
 * ps2_probe() đọc ra trạng thái đó để kiểm, và hỏi bàn phím đang dùng scancode set nào.
 */
#include "ps2.h"
#include "io.h"
#include "serial.h"

#define TIMEOUT 100000              /* như MOUSE_TIMEOUT của cavOS: đếm vòng, không phải ms */

static int wait_write(void) {       /* chờ IBF = 0 rồi mới được ghi 0x60/0x64 */
    for (int t = TIMEOUT; t; t--)
        if (!(inb(PS2_STATUS) & PS2_ST_IBF))
            return 1;
    return 0;
}

static int wait_read(void) {        /* chờ OBF = 1 mới có byte để đọc 0x60 */
    for (int t = TIMEOUT; t; t--)
        if (inb(PS2_STATUS) & PS2_ST_OBF)
            return 1;
    return 0;
}

static int read_byte(uint8_t *out) {
    if (!wait_read())
        return 0;
    *out = inb(PS2_DATA);
    return 1;
}

uint8_t ps2_read_config(void) {
    wait_write();
    outb(PS2_COMMAND, PS2_CMD_READ_CONFIG);
    uint8_t v = 0;
    read_byte(&v);
    return v;
}

void ps2_write_config(uint8_t cfg) {
    wait_write();
    outb(PS2_COMMAND, PS2_CMD_WRITE_CONFIG);
    wait_write();
    outb(PS2_DATA, cfg);
}

void ps2_print_status(const char *label, uint8_t st) {
    serial_puts(label);
    serial_puthex_short(st);
    serial_puts(" (OBF=");
    serial_putdec(st & 1);
    serial_puts(" IBF=");
    serial_putdec((st >> 1) & 1);
    serial_puts(" SYS=");
    serial_putdec((st >> 2) & 1);
    serial_puts(" CMD=");
    serial_putdec((st >> 3) & 1);
    serial_puts(" AUX=");
    serial_putdec((st >> 5) & 1);
    serial_puts(")\n");
}

void ps2_print_config(const char *label, uint8_t cfg) {
    serial_puts(label);
    serial_puthex_short(cfg);
    serial_puts(" (IRQ1=");
    serial_putdec(cfg & 1);
    serial_puts(" IRQ12=");
    serial_putdec((cfg >> 1) & 1);
    serial_puts(" SYS=");
    serial_putdec((cfg >> 2) & 1);
    serial_puts(" port1-clock-off=");
    serial_putdec((cfg >> 4) & 1);
    serial_puts(" port2-clock-off=");
    serial_putdec((cfg >> 5) & 1);
    serial_puts(" translate=");
    serial_putdec((cfg >> 6) & 1);
    serial_puts(")\n");
}

/* Lệnh bàn phím 0xF0 0x00 = "get scancode set": bàn phím trả ACK 0xFA rồi số set. */
static void query_set(const char *label) {
    uint8_t ack1 = 0, ack2 = 0, set = 0;
    wait_write();
    outb(PS2_DATA, 0xF0);
    int ok = read_byte(&ack1);
    wait_write();
    outb(PS2_DATA, 0x00);
    ok = ok && read_byte(&ack2) && read_byte(&set);
    serial_puts(label);
    if (!ok) {
        serial_puts("timeout\n");
        return;
    }
    serial_puts("ACK ");
    serial_puthex_short(ack1);
    serial_puts(", ACK ");
    serial_puthex_short(ack2);
    serial_puts(", reply ");
    serial_puthex_short(set);
    serial_puts(set == 0x43 ? " (= set 1 after translation)\n"
              : set == 0x41 ? " (= set 2 after translation)\n"
              : set == 0x3f ? " (= set 3 after translation)\n"
              : "\n");
}

void ps2_probe(void) {
    ps2_print_status("[8042] status at boot      = ", inb(PS2_STATUS));

    int flushed = 0;                       /* byte còn sót từ firmware/bootloader */
    while ((inb(PS2_STATUS) & PS2_ST_OBF) && flushed < 16) {
        inb(PS2_DATA);
        flushed++;
    }
    serial_puts("[8042] stale bytes flushed = ");
    serial_putdec(flushed);
    serial_putc('\n');

    uint8_t cfg = ps2_read_config();
    ps2_print_config("[8042] config left by firmware = ", cfg);

    query_set("[8042] kb 0xF0 0x00 (get scancode set), translate=1: ");
    ps2_write_config(cfg & ~PS2_CFG_TRANSLATE);
    query_set("[8042] kb 0xF0 0x00 (get scancode set), translate=0: ");
    ps2_write_config(cfg);                 /* trả lại như firmware để, cavOS dựa vào nó */
}
