/* fb.h — framebuffer từ Limine (Step 03).
 * Pixel format BGRX 32bpp; nhảy hàng bằng `pitch` (KHÔNG phải width*4).
 */
#pragma once
#include <stdint.h>

struct fb_info {
    volatile uint8_t *virt;   /* địa chỉ ẢO Limine trả về (= phys + hhdm) */
    uint64_t width;
    uint64_t height;
    uint64_t pitch;           /* số byte mỗi hàng, có thể > width*4 (padding) */
    uint16_t bpp;
};

int  fb_init(void);
const struct fb_info *fb_get(void);

void fb_put_pixel(uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b);
void fb_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h,
                  uint8_t r, uint8_t g, uint8_t b);
void fb_draw_string(uint64_t x, uint64_t y, const char *s, uint32_t scale,
                    uint8_t r, uint8_t g, uint8_t b);
