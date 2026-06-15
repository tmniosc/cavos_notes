---
tags: [step, boot, serial]
status: done
---

# Step 01 — Serial UART (COM1)

**File:** `entry/serial.c` (hoặc tương đương) · **Spec:** [[UART 16550]]
**🔬 Thực hành:** [[Lab 0x00 - Hello Serial]] (gộp Step 00 + 01).

## 🎯 Mục đích
**Có kênh xuất log SỚM NHẤT có thể** để debug mọi bước sau. Serial chỉ cần vài thanh ghi I/O port (không
cần framebuffer/console/paging) → init được ngay đầu `_start`. QEMU nối COM1 ra stdout host → thấy log
kernel trực tiếp. Đây là "đèn pin" soi đường cho toàn bộ hành trình còn lại.

### 🧒 Nói thật đơn giản
Kernel chưa có màn hình, chưa có gì để "in ra" cho bạn xem nó đang làm gì. **Serial (cổng COM)** là cách
in chữ ra dễ nhất: chỉ cần ghi byte vào một "ống" phần cứng, QEMU hứng lấy rồi hiện trên terminal của bạn.
→ Làm serial **đầu tiên** để từ đó về sau, bước nào lỗi cũng in ra xem được. Giống bật đèn trước khi vào phòng tối.

Bài này làm 2 việc nhỏ:
- **Cài đặt cổng** một lần: nói cho chip serial biết tốc độ truyền và định dạng (kiểu "chỉnh đài cho đúng
  tần số") bằng cách ghi vào vài thanh ghi của nó.
- **Gửi từng ký tự**: trước khi gửi, hỏi chip "đã rảnh chưa?" rồi mới đẩy byte ra. Lặp lại cho cả chuỗi.

Chỉ vậy thôi — serial đơn giản nên mới được chọn làm kênh log đầu tiên. (Tên/địa chỉ từng thanh ghi là
phần kỹ thuật, xem [[UART 16550]] khi cần.)

`initiateSerial()` là hàm **đầu tiên** `_start()` gọi (xem [[Boot Flow (_start)]]) — để có kênh debug
in log ra trước khi mọi hệ con khác chạy.

## Vì sao serial trước tiên?
- Không cần framebuffer/console (chưa init). Chỉ cần vài thanh ghi I/O port.
- QEMU chuyển COM1 ra stdout/file → `printf` kernel hiện ngay trên host.

## Khởi tạo COM1 (base `0x3F8`, chuẩn 8N1, 115200)
> _"Chỉnh đài" cho cổng serial: đặt tốc độ và định dạng truyền, làm đúng 1 lần lúc đầu. Mỗi dòng dưới là
> ghi một con số vào một nút chỉnh của chip._

Thứ tự ghi thanh ghi (offset từ base) — xem chi tiết bảng thanh ghi ở [[UART 16550]]:
```c
outb(PORT+1, 0x00);  // IER = 0: tắt mọi interrupt
outb(PORT+3, 0x80);  // LCR: bật DLAB để set baud divisor
outb(PORT+0, 0x03);  // DLL: divisor low  = 3 -> 115200/3 = 38400 (tùy cấu hình)
outb(PORT+1, 0x00);  // DLM: divisor high = 0
outb(PORT+3, 0x03);  // LCR: 8 bit, no parity, 1 stop (8N1), tắt DLAB
outb(PORT+2, 0xC7);  // FCR: bật FIFO, clear, ngưỡng 14 byte
outb(PORT+4, 0x0B);  // MCR: IRQs enable, RTS/DSR set
```

## Truyền 1 byte
> _Gửi 1 ký tự: chờ chip báo "đã gửi xong cái trước, rảnh rồi" rồi mới đẩy byte mới vào. Không chờ thì chữ
> chồng lên nhau, mất._

```c
while ((inb(PORT+5) & 0x20) == 0);  // chờ LSR bit5 (THR empty)
outb(PORT+0, c);                    // ghi vào THR
```
- **LSR (offset 5) bit5 = THRE**: thanh ghi truyền rỗng → an toàn ghi byte tiếp.

## Ánh xạ spec
- Thanh ghi & bit: [[UART 16550]] (datasheet 16550A).
- `inb`/`outb`: port I/O x86, [[Intel SDM]] Vol.1.
