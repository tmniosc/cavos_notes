/* kb.c — bàn phím PS/2, theo cavOS drivers/kb.c (Bài 11).
 *
 * Giống cavOS:
 *  - initiateKb(): devInputEventSetup("PS/2 Keyboard") -> ioApicRedirect(1) ->
 *    đăng ký kbIrq -> lệnh 0xAE (bật cổng 1) -> đọc bỏ 0x60.
 *  - Không chọn scancode set: dùng set 1 vì controller dịch set 2 -> set 1 (bit 6 byte
 *    cấu hình, firmware bật sẵn). Bảng characterTable/shiftedCharacterTable chép nguyên.
 *  - kbEvdevGenerate(): bitmap nhấn/nhả -> EV_KEY value 1 (nhấn) / 2 (lặp) / 0 (nhả) + EV_SYN.
 * Khác cavOS (ghi trên trang lab):
 *  - Mỗi byte tính ký tự theo 2 luật: luật cavOS (cavos_rule, chép nguyên handleKbEvent)
 *    và luật của lab (lab_rule): Caps Lock chỉ đổi chữ cái, có Shift phải, bỏ qua byte
 *    sau tiền tố 0xE0. Log in cả hai khi khác nhau.
 *  - Ký tự đi vào tty_q (ring) thay vì kbBuff của task đang read().
 */
#include "kb.h"
#include "apic.h"
#include "io.h"
#include "isr.h"
#include "ps2.h"

/* ---------------------------------------------- bảng của cavOS, chép nguyên */
static const char characterTable[] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',
    '-',  '=',  0,    9,    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
    'o',  'p',  '[',  ']',  0,    0,    'a',  's',  'd',  'f',  'g',  'h',
    'j',  'k',  'l',  ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  0,    ' ',  0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0x1B, 0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,    0,    '/',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0,
    0,    0,    0,    0,    0,    0,    0,    0x2C,
};

static const char shiftedCharacterTable[] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',
    '_',  '+',  0,    9,    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
    'O',  'P',  '{',  '}',  0,    0,    'A',  'S',  'D',  'F',  'G',  'H',
    'J',  'K',  'L',  ':',  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  0,    ' ',  0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0x1B, 0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,    0,    '?',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0,
    0,    0,    0,    0,    0,    0,    0,    0x2C,
};

/* evdevTable của cavOS viết bằng tên KEY_*; giá trị số của chúng (Linux
 * input-event-codes.h) TRÙNG scancode set 1 từ 1 tới 88, trừ các phím keypad mà cavOS
 * cố ý đổi thành phím mũi tên / Insert / Delete, và 84-86 để trống. */
static const uint8_t evdevTable[89] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71,
    103,            /* 72 KP8 -> KEY_UP */
    73, 74,
    105,            /* 75 KP4 -> KEY_LEFT */
    76,
    106,            /* 77 KP6 -> KEY_RIGHT */
    78, 79,
    108,            /* 80 KP2 -> KEY_DOWN */
    81,
    110,            /* 82 KP0 -> KEY_INSERT */
    111,            /* 83 KPDOT -> KEY_DELETE */
    0, 0, 0,        /* 84-86 */
    87, 88,         /* F11, F12 */
};

DevInputEvent *kbEvent;

/* ------------------------------------------------ kbEvdevGenerate (y cavOS) */
static uint8_t evdevInternal[(89 + 7) / 8];   /* bitmap: phím nào đang được giữ */
static uint8_t lastPressed;

static int  bget(int i)        { return (evdevInternal[i / 8] >> (i % 8)) & 1; }
static void bset(int i, int v) {
    if (v) evdevInternal[i / 8] |= (uint8_t)(1 << (i % 8));
    else   evdevInternal[i / 8] &= (uint8_t)~(1 << (i % 8));
}

static void kbEvdevGenerate(uint8_t raw) {
    uint8_t index = 0;
    int clicked = 0;
    if (raw <= 0x58) {              /* make code */
        clicked = 1;
        index = raw;
    } else if (raw <= 0xD8) {       /* break code = make | 0x80 */
        clicked = 0;
        index = raw - 0x80;
    } else
        return;                     /* 0xE0, 0xE1, 0xFA... */

    if (index > 88)
        return;
    uint8_t evdevCode = evdevTable[index];
    if (!evdevCode)
        return;

    int oldstate = bget(index);
    if (!oldstate && clicked) {                 /* nhấn */
        inputGenerateEvent(kbEvent, EV_KEY, evdevCode, 1);
        lastPressed = evdevCode;
    } else if (oldstate && clicked) {           /* đang giữ mà lại nhận make = lặp */
        if (evdevCode != lastPressed)
            return;
        inputGenerateEvent(kbEvent, EV_KEY, evdevCode, 2);
    } else if (oldstate && !clicked) {          /* nhả */
        inputGenerateEvent(kbEvent, EV_KEY, evdevCode, 0);
    }
    inputGenerateEvent(kbEvent, EV_SYN, SYN_REPORT, 0);
    bset(index, clicked);
}

/* ------------------------------------------- luật cavOS (handleKbEvent nguyên) */
static int shifted, capsLocked;

static char cavos_rule(uint8_t scanCode) {
    if (shifted == 1 && (scanCode & 0x80)) {
        if ((scanCode & 0x7F) == SCANCODE_SHIFT) {
            shifted = 0;
            return 0;
        }
    }
    if (scanCode < sizeof(characterTable) && !(scanCode & 0x80)) {
        char character = (shifted || capsLocked) ? shiftedCharacterTable[scanCode]
                                                 : characterTable[scanCode];
        if (character != 0)
            return character;
        switch (scanCode) {
        case SCANCODE_ENTER: return CHARACTER_ENTER;
        case SCANCODE_BACK:  return CHARACTER_BACK;
        case SCANCODE_SHIFT: shifted = 1; break;
        case SCANCODE_CAPS:  capsLocked = !capsLocked; break;
        }
    }
    return 0;
}

/* --------------------------------------------------------- luật của lab */
static int lshift, rshift, caps, e0;

static char lab_rule(uint8_t sc) {
    if (sc == 0xE0) {               /* tiền tố: byte sau là phím "mở rộng" (mũi tên...) */
        e0 = 1;
        return 0;
    }
    int ext = e0;
    e0 = 0;
    uint8_t key = sc & 0x7F;
    int make = !(sc & 0x80);
    if (sc >= 0xE1)                 /* 0xE1 (Pause), 0xFA ACK, 0xFE...: không phải phím */
        return 0;
    if (!ext && key == SCANCODE_SHIFT)  { lshift = make; return 0; }
    if (!ext && key == SCANCODE_RSHIFT) { rshift = make; return 0; }
    if (!make)
        return 0;
    if (ext)                        /* E0 1C = Enter keypad, E0 35 = '/' keypad, còn lại không có ASCII */
        return key == 0x1C ? '\n' : key == 0x35 ? '/' : 0;
    if (key == SCANCODE_CAPS)  { caps = !caps; return 0; }
    if (key == SCANCODE_ENTER) return CHARACTER_ENTER;
    if (key == SCANCODE_BACK)  return CHARACTER_BACK;
    if (key > 0x39)                 /* ngoài hàng phím chính: F1.., keypad */
        return 0;
    char n = characterTable[key];
    if (!n)
        return 0;
    int shift = lshift || rshift;
    int letter = (n >= 'a' && n <= 'z');
    int upper = letter ? (shift ^ caps) : shift;   /* Caps chỉ đổi chữ cái */
    return upper ? shiftedCharacterTable[key] : n;
}

/* ------------------------------------------------------------- IRQ1 */
static uint8_t kbRead(uint8_t *status) {        /* = cavOS kbRead(): chờ OBF rồi đọc */
    uint8_t st;
    while (!((st = inb(PS2_STATUS)) & PS2_ST_OBF))
        ;
    *status = st;
    return inb(PS2_DATA);
}

static void kbWrite(uint16_t port, uint8_t value) {   /* = cavOS kbWrite(): chờ IBF = 0 */
    while (inb(PS2_STATUS) & PS2_ST_IBF)
        ;
    outb(port, value);
}

static void kbIrq(AsmPassedInterrupt *regs) {
    (void)regs;
    ps2_trace t = {0};
    t.kind = TRACE_KB;
    uint8_t scanCode = kbRead(&t.status[0]);
    t.byte[0] = scanCode;
    kbEvdevGenerate(scanCode);                  /* -> /dev/input/event0 */
    t.ch_cavos = cavos_rule(scanCode);
    t.ch = lab_rule(scanCode);
    ring_put(&trace_q, &t);
    if (t.ch)
        ring_put(&tty_q, &t.ch);                /* cavOS: kbChar() -> kbBuff[kbCurr++] */
}

void initiateKb(void) {
    kbEvent = devInputEventSetup("PS/2 Keyboard");
    uint8_t targIrq = ioApicRedirect(1, 0);
    register_irq_handler(targIrq, kbIrq);
    kbWrite(PS2_COMMAND, PS2_CMD_ENABLE_PORT1); /* 0xAE */
    inb(PS2_DATA);                              /* đọc bỏ: byte cũ sẽ chặn cạnh IRQ mới */
}
