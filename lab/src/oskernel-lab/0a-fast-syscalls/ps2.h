/* ps2.h — controller 8042 (Bài 11): cổng, bit trạng thái, byte cấu hình.
 *
 * cavOS không có file riêng cho controller: kb.c có kbRead/kbWrite, mouse.c có
 * mouseWait/mouseWrite/mouseRead, cả hai dùng thẳng số 0x60/0x64. Lab gom tên hằng vào
 * đây, cộng vài hàm chỉ để quan sát (ps2_probe), cavOS không làm.
 */
#pragma once
#include <stdint.h>

#define PS2_DATA    0x60    /* đọc: byte từ thiết bị/controller; ghi: byte gửi thiết bị */
#define PS2_STATUS  0x64    /* đọc: thanh ghi trạng thái */
#define PS2_COMMAND 0x64    /* ghi: lệnh cho chính controller */

/* Thanh ghi trạng thái (đọc 0x64) */
#define PS2_ST_OBF  0x01    /* bit 0: output buffer full -> có byte chờ ở 0x60 */
#define PS2_ST_IBF  0x02    /* bit 1: input buffer full -> controller chưa nhận xong, chưa được ghi */
#define PS2_ST_SYS  0x04    /* bit 2: system flag (POST đã qua) */
#define PS2_ST_CMD  0x08    /* bit 3: byte cuối ghi vào là lệnh (0x64) hay dữ liệu (0x60) */
#define PS2_ST_AUX  0x20    /* bit 5: byte ở 0x60 đến từ cổng 2 (chuột) */

/* Lệnh controller (ghi 0x64) — cavOS dùng 0xAE, 0xA8, 0x20, 0x60, 0xD4 */
#define PS2_CMD_READ_CONFIG  0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_ENABLE_PORT2 0xA8
#define PS2_CMD_ENABLE_PORT1 0xAE
#define PS2_CMD_WRITE_PORT2  0xD4   /* byte kế tiếp ghi vào 0x60 đi tới chuột */

/* Byte cấu hình (đọc bằng 0x20, ghi bằng 0x60) */
#define PS2_CFG_IRQ1      0x01      /* bit 0: cổng 1 có byte -> IRQ1 */
#define PS2_CFG_IRQ12     0x02      /* bit 1: cổng 2 có byte -> IRQ12 */
#define PS2_CFG_SYS       0x04
#define PS2_CFG_CLK1_OFF  0x10      /* bit 4: tắt clock cổng 1 */
#define PS2_CFG_CLK2_OFF  0x20      /* bit 5: tắt clock cổng 2 */
#define PS2_CFG_TRANSLATE 0x40      /* bit 6: dịch scancode set 2 -> set 1 */

void    ps2_print_status(const char *label, uint8_t st);
void    ps2_print_config(const char *label, uint8_t cfg);
uint8_t ps2_read_config(void);
void    ps2_write_config(uint8_t cfg);
void    ps2_probe(void);          /* lab: trạng thái, byte cấu hình, scancode set có/không dịch */
