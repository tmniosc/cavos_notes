---
tags: [concept, boot]
---

# Request-Response Mechanism

> Cách [[Limine Protocol|Limine]] truyền dữ liệu cho kernel. Ý tưởng: **phong bì có nhãn**.

## Ẩn dụ
Kernel để sẵn các "phong bì có nhãn" trong file nhị phân ("tôi muốn biết X"). Nhãn = **dãy số magic
64-bit** (gần như không trùng tình cờ). Limine quét nhị phân kernel, tìm nhãn, rồi nhét "tờ trả lời"
(response) vào phong bì. Kernel chạy thì mở ra đọc.

## Ví dụ thật — HHDM (`limine.h`)
```c
#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b
#define LIMINE_HHDM_REQUEST { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }

struct limine_hhdm_request {
    uint64_t id[4];        // nhãn (2 magic chung + 2 magic riêng)
    uint64_t revision;
    struct limine_hhdm_response *response;  // ban đầu NULL; Limine điền vào
};
struct limine_hhdm_response { uint64_t revision; uint64_t offset; };
```
Trong `entry/bootloader.c`:
```c
static volatile struct limine_hhdm_request limineHHDMreq = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0};      // response còn NULL
...
struct limine_hhdm_response *res = limineHHDMreq.response;  // Limine đã điền
bootloader.hhdmOffset = res->offset;
```

## Dòng thời gian
```
Biên dịch : request có id=magic, response=NULL nằm trong nhị phân kernel
Limine nạp: quét thấy magic → tạo response → gán địa chỉ vào request.response
_start    : request.response đã trỏ dữ liệu thật → kernel đọc
```

## Vì sao `static volatile`?
`volatile` báo compiler "biến bị bên ngoài (Limine) sửa" → không tối ưu/cache, không xóa code đọc
`response`. `static` để biến tồn tại trong nhị phân cho Limine quét.

→ Cùng cơ chế này áp dụng cho [[Limine Protocol|base revision]].
