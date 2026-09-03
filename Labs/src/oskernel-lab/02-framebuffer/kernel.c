/* Lab 0x02 — Framebuffer (Step 03).
 * Lấy framebuffer Limine, in thông số ra serial rồi vẽ: nền xám, 3 thanh RGB,
 * ô vuông trắng, chữ 'HELLO FB'. Chứng minh BGRX + pitch + HHDM.
 */
#include "../limine.h"
#include "fb.h"
#include "serial.h"

LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    serial_puts("=== Lab 0x02 - Framebuffer (Step 03) ===\n");

    if (!fb_init()) {
        serial_puts("[fb] no framebuffer response\n");
        halt();
    }

    const struct fb_info *f = fb_get();

    serial_puts("[fb] address(VA, qua HHDM) = ");
    serial_puthex((uint64_t)f->virt);
    serial_puts("\n[fb] resolution = ");
    serial_putdec(f->width);
    serial_puts(" x ");
    serial_putdec(f->height);
    serial_puts("\n[fb] pitch = ");
    serial_putdec(f->pitch);
    serial_puts(" bytes/row   (width*4 = ");
    serial_putdec(f->width * 4);
    serial_puts(")\n[fb] bpp = ");
    serial_putdec(f->bpp);
    serial_putc('\n');

    if (f->pitch == f->width * 4)
        serial_puts("[fb] -> pitch == width*4 (no padding)\n");
    else
        serial_puts("[fb] -> pitch > width*4 (row padding! phai dung pitch)\n");

    /* nền xám */
    fb_fill_rect(0, 0, f->width, f->height, 0x20, 0x20, 0x28);

    /* 3 thanh màu R/G/B trên đỉnh — đảo p[0]/p[1]/p[2] trong fb.c sẽ thấy đổi màu */
    uint64_t bar_w = f->width / 3;
    fb_fill_rect(0,          0, bar_w, 60, 0xFF, 0x00, 0x00);
    fb_fill_rect(bar_w,      0, bar_w, 60, 0x00, 0xFF, 0x00);
    fb_fill_rect(bar_w * 2,  0, f->width - bar_w * 2, 60, 0x00, 0x00, 0xFF);

    /* ô vuông trắng giữa màn hình */
    fb_fill_rect(f->width / 2 - 60, f->height / 2 - 60, 120, 120, 0xFF, 0xFF, 0xFF);

    /* chữ vàng, font 8x8 scale 4 */
    fb_draw_string(f->width / 2 - 128, f->height / 2 + 100, "HELLO FB", 4,
                   0xFF, 0xD7, 0x00);

    serial_puts("[fb] drawn: 3 color bars + white square + text 'HELLO FB'.\n");
    serial_puts("[done] halt.\n");

    halt();
}
