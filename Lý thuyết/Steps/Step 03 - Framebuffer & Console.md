---
tags: [step, video]
status: done
---

# Step 03 — Framebuffer & Console

**Files:** `drivers/vga.c`, `graphical/fb.c`, `graphical/console.c`, `utilities/psf.c`
**Headers:** `include/fb.h`, `include/vga.h`, `include/console.h`, `include/psf.h`
**Khái niệm:** [[Limine Protocol]] (`limine_framebuffer_request`), [[HHDM]] (framebuffer là MMIO map sẵn HHDM)
**🔬 Thực hành:** [[Lab 0x02 - Framebuffer]] (vẽ pixel/chữ, xác nhận HHDM bằng địa chỉ fb thật).

## 🎯 Mục đích
**Vẽ được lên màn hình + có console text** (ngoài serial). Lấy framebuffer Limine cung cấp, hiểu cách
ghi pixel (BGRX, pitch), rồi xây tầng console: font bitmap → ký tự → con trỏ → scroll → `printf`. Kết quả:
mọi `printf` của kernel hiện lên màn hình, không chỉ serial. Đồng thời là minh hoạ thực tế của [[HHDM]]
(framebuffer là MMIO map sẵn ở vùng HHDM).

### 🧒 Nói thật đơn giản
Màn hình thực ra là **một mảng pixel nằm trong bộ nhớ** (gọi là *framebuffer*). Muốn vẽ một điểm sáng ở vị
trí (x,y) → chỉ cần ghi mấy byte màu vào đúng ô đó trong mảng. Limine đưa cho kernel **địa chỉ của mảng này**.

Cả bài đi từ thấp lên cao:
- **Vẽ 1 pixel**: ghi màu (đỏ/lục/lam) vào ô của pixel. Lưu ý mỗi hàng pixel cách nhau một bước cố định
  (*pitch*) — phải nhảy đúng bước này kẻo ảnh bị xô lệch.
- **Vẽ hình**: tô nhiều pixel = hình chữ nhật, xoá màn, v.v.
- **Viết chữ**: mỗi chữ cái có sẵn "khuôn" dạng lưới chấm (*font*); chấm nào bật thì tô pixel màu → ra chữ.
- **Console**: ghép việc viết chữ + con trỏ + xuống dòng + cuộn màn hình. Từ đó `printf` của kernel hiện
  thẳng lên màn hình, không chỉ qua cổng serial.

Điểm hay: địa chỉ màn hình Limine đưa là **địa chỉ kernel dùng được ngay** (qua HHDM) — ghi vào đó là thấy
trên màn hình. (Chi tiết định dạng màu, font PSF... ở phần kỹ thuật bên dưới.)

Trong `_start()` (xem [[Boot Flow (_start)]]), ngay sau bootloader parser:
```c
// Framebuffer doesn't depend on paging, limine prepares it anyways
initiateVGA();       // drivers/vga.c   — lấy framebuffer từ Limine
initiateConsole();   // graphical/console.c — nạp font, sẵn sàng vẽ chữ
clearScreen();
```

## 3.1 `initiateVGA()` — thu hoạch framebuffer (`vga.c`)
> _Hỏi Limine "màn hình ở đâu, bao to" rồi chép thông tin vào struct `fb` để cả kernel dùng._

```c
fb.virt = (uint8_t *)framebufferRes->address;       // ⚠️ Limine trả VA (đã qua HHDM!)
fb.phys = (size_t)fb.virt - bootloader.hhdmOffset;  // trừ offset → ra PA
fb.height/width/pitch/bpp = ...;
fb.red_shift/red_size, green_*, blue_* = ...;        // vị trí bit từng kênh màu
```
- **`address` là địa chỉ ẢO**: framebuffer là vùng **MMIO**, Limine đã map vào [[HHDM]] → ghi `fb.virt[...]`
  là **ghi thẳng vào VRAM** qua HHDM. Đúng bài [[HHDM]] mục "dùng VA nào": cầm vùng nhớ vật lý → dùng VA HHDM.
- Lưu cả `phys` (tính ngược) để sau này userspace `mmap` framebuffer cần địa chỉ vật lý (xem 3.6).
- "Không phụ thuộc paging": Limine đã bật paging + map HHDM trước `_start`, nên dùng được ngay dù PMM/VMM
  của kernel chưa chạy.

## 3.2 Struct `Framebuffer` (`fb.h`) + macro `drawPixel`
> _Định nghĩa "tờ giấy vẽ" (kích thước, vị trí) và hàm chấm 1 điểm màu. Mỗi pixel = 4 byte theo thứ tự xanh
> dương–lục–đỏ–thừa._

```c
typedef struct Framebuffer {
  uint8_t *virt; size_t phys;
  size_t width, height, bpp, pitch;     // pitch = số BYTE mỗi hàng (≥ width*4)
  size_t red_shift, red_size, ...;
} Framebuffer;
Framebuffer fb;

#define drawPixel(x, y, r, g, b) do {                 \
    fb.virt[((x)+(y)*fb.width)*4]   = (b);            \
    fb.virt[((x)+(y)*fb.width)*4+1] = (g);            \
    fb.virt[((x)+(y)*fb.width)*4+2] = (r);            \
    fb.virt[((x)+(y)*fb.width)*4+3] = 0;              \
  } while (0)
```
- Mỗi pixel **4 byte**, thứ tự **B, G, R, X** (BGRX little-endian = `0x00RRGGBB`) — chuẩn 32bpp của GOP.
- Địa chỉ pixel `(x,y)` = `virt + (x + y*width)*4`.

> ⚠️ **Cạm bẫy pitch:** macro `drawPixel` dùng `fb.width`, còn `drawRect`/`scrollConsole` dùng `fb.pitch`.
> Nếu `pitch == width*4` (QEMU thường vậy) thì hai cách trùng. Nhưng nếu **pitch > width*4** (hàng có
> byte padding cuối dòng), `drawPixel` sẽ tính lệch dòng → ảnh xô chéo. Cách đúng tổng quát: **luôn dùng
> `pitch` cho bước nhảy hàng**, chỉ `*4` cho bước nhảy cột.

## 3.3 Vẽ hình khối: `drawRect` (`fb.c`)
> _Tô một hình chữ nhật đầy màu — dùng để xoá màn, vẽ nền, vẽ/xoá con trỏ._

```c
void drawRect(int x, int y, int w, int h, int r, int g, int b) {
  unsigned int offset = (x + y * fb.width) * 4;
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {        // tô từng pixel trong hàng
      fb.virt[offset + j*4]   = b; ...   // B G R X
    }
    offset += fb.pitch;                  // ✅ sang hàng kế bằng PITCH
  }
}
```
Dùng cho: `clearScreen` (tô cả màn nền), xóa/vẽ con trỏ (`eraseBull`/`updateBull`), nền khi scroll.

## 3.4 Font PSF1 & vẽ ký tự (`psf.c`)
> _Mỗi chữ cái là một "khuôn lưới chấm" 8×cao. Vẽ chữ = quét từng chấm: chấm bật thì tô pixel màu chữ, tắt
> thì tô màu nền._

Console dùng **font bitmap PSF1** (nhúng sẵn `gohufont.h` → `u_vga16_psf`):
```c
typedef struct PSF1Header { uint16_t magic; uint8_t mode; uint8_t height; } PSF1Header;
// magic = 0x0436, width LUÔN = 8px, height tùy font (vd 16)
```
`psfLoadDefaults()` trỏ `psf` vào font nhúng (kiểm magic `0x0436`). Vẽ 1 ký tự:
```c
void psfPutC(char c, uint32_t x, uint32_t y, r, g, b) {
  uint8_t *glyph = (uint8_t*)psf + sizeof(PSF1Header) + c * psf->height; // bitmap ký tự c
  for (i = 0; i < psf->height; i++)        // mỗi hàng = 1 byte (8 bit = 8 cột)
    for (j = 0; j < 8; j++)
      if (glyph[i] & (1 << (8 - j)))       // bit set → pixel chữ
        drawPixel(x+j, y+i, r, g, b);
      else
        drawPixel(x+j, y+i, bg...);        // bit clear → pixel nền
}
```
- Mỗi glyph = `height` byte liên tiếp; byte thứ `i` là **hàng pixel i**, 8 bit là 8 cột.
- `1 << (8-j)` (không phải `7-j`) — tác giả tự nhận "NOT little endian" trong comment.

## 3.5 Console — máy trạng thái con trỏ text (`console.c`)
> _Giữ vị trí con trỏ rồi xử lý từng ký tự: chữ thường thì vẽ + dịch con trỏ; `\n` xuống dòng; hết màn thì
> cuộn lên. Đây là tầng biến "vẽ chữ" thành "gõ văn bản như terminal"._

Biến toàn cục `width`/`height` = **vị trí con trỏ tính bằng PIXEL** (không phải ô chữ). `CHAR_WIDTH=8`,
`CHAR_HEIGHT=psf->height`.

`drawCharacter(c)` xử lý:

| char | hành vi |
|---|---|
| `\n` | xóa con trỏ, `width=0`, `height += CHAR_HEIGHT` (xuống dòng) |
| `\r` (0xd) | `width=0` (về đầu dòng) |
| `\b` | lùi 1 ô, tô nền |
| `\t` | 4 dấu cách |
| khác | `psfPutC(c, width, height, ...)` rồi `width += CHAR_WIDTH` |

- **Wrap dòng**: nếu `width > fb.width - CHAR_WIDTH` → về `width=0`, `height += CHAR_HEIGHT`.
- **Cuộn** `scrollConsole`: khi chạm đáy, `memcpy` dịch toàn bộ ảnh lên 1 dòng chữ rồi xóa dòng cuối.
- **Con trỏ** (`updateBull`/`eraseBull`): vẽ/xóa khối nhỏ tại vị trí hiện tại.
- **Khóa**: `printfch` bọc `spinlockAcquire(LOCK_CONSOLE)` → an toàn đa luồng (sau này SMP).

### Mạch nối printf → màn hình
```
printf(...) → (printf.c) → putchar_(c) → printfch(c) → drawCharacter(c) → psfPutC → drawPixel → fb.virt (VRAM)
```
`putchar_` (console.c) chính là hàm `printf.c` gọi → mọi `printf` của kernel hiện lên màn hình.

## 3.6 Framebuffer cho userspace (`fb0`, `fb.c`) — ngoài phạm vi Step 03
> _Sau này chương trình người dùng (vd Xorg) muốn tự vẽ lên màn hình; phần này mở "cửa" cho nó xin map
> framebuffer. Chưa cần ở Step 03, ghi để biết._

`fb.c` còn export `VfsHandlers fb0` để userspace `ioctl`/`mmap` framebuffer:
- `FBIOGET_VSCREENINFO`/`FSCREENINFO`: trả độ phân giải, bitfield màu, `smem_start = fb.phys`.
- `fbUserMmap`: map `fb.phys` vào VA userspace `0x150000000000` với `PF_RW|PF_USER|PF_CACHE_WC`
  (write-combining cho VRAM). → liên hệ [[Step 17 - Userspace & ELF Loader]], [[Step 05 - Virtual Memory & Paging]].

## ✅ Đã trả lời (câu hỏi mở cũ)
- **FB ở VA nào?** → VA HHDM (`framebufferRes->address`); `fb.phys = virt − hhdmOffset`.
- **Format pixel?** → 32bpp **BGRX** (`0x00RRGGBB`), kèm `red/green/blue_shift+size` từ Limine.
- **Scroll?** → `scrollConsole`: `memcpy` dịch ảnh lên 1 dòng chữ, xóa dòng đáy.

## Ánh xạ spec
- Limine framebuffer response (`address`, `pitch`, `bpp`, `*_mask_shift/size`): [[Danh mục tài liệu]].
- PSF1 font format (magic `0x0436`, header 4 byte, glyph 8×height): PSF spec.
- Pixel BGRX/GOP: UEFI GOP `PixelBlueGreenRedReserved8BitPerColor`.
