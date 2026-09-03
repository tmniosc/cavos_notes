---
tags: [moc, thuchanh]
---

# Thực hành

> _Nửa "tự làm cho chạy" của vault: viết kernel từ số 0 trong `~/oskernel-lab`, build và boot QEMU thật.
> Nửa còn lại là [[Lý thuyết]] — đọc code cavOS và spec._

Về trang chủ: [[Home]]

## Vì sao lab không map 1:1 với Step

> _Nhiều Step phụ thuộc nhau, tách lẻ ra thì mỗi lab chẳng chạy được gì để mà xem._

Lab gộp theo **cụm "chạy thấy được"**. Tiêu chí: **1 lab = 1 thứ quan sát được bằng mắt** — chữ hiện ra
serial, hình vẽ lên màn hình, một con số in ra chứng minh điều vừa học.

| Lab | Phụ trách Step | Thấy được gì |
| --- | --- | --- |
| [[Lab 0x00 - Hello Serial]] | 00 + 01 | dòng chữ đầu tiên chui ra cổng COM1 |
| [[Lab 0x01 - Bootloader Parser]] | 02 | HHDM offset, địa chỉ kernel, bản đồ bộ nhớ vật lý |
| [[Lab 0x02 - Framebuffer]] | 03 | pixel và chữ vẽ lên màn hình |
| [[Lab 0x03 - PMM & VMM]] | 04 + 05 | cấp phát frame, tự dựng page table, `mov cr3` |
| 0x04 (chưa làm) | 06 → 09 | GDT/TSS, ngắt, timer |

## Nguyên tắc

> _Ba điều đã trả giá mới rút ra được._

- **Phải build + boot QEMU thật rồi mới chép output vào note.** Không bịa output, không bịa bài học —
  đã từng sai, xem mục "Ghi chú trung thực" trong [[Lab 0x03 - PMM & VMM]].
- **Tách module từ Lab 0x02 trở đi**: `io.h` / `serial.{h,c}` / `boot.{h,c}` / `pmm.{h,c}` / `paging.{h,c}`,
  còn `kernel.c` chỉ điều phối. String in ra để **tiếng Anh**, comment giải thích tiếng Việt.
- **Bám cách cavOS thật làm**, không tự chế kiểu khác — ví dụ PMM tính `mmTotal` bằng cộng dồn length các
  vùng khác `RESERVED`, y như `bootloader.c`.

## Source và cách chạy

```
Thực hành/src/oskernel-lab/     ← bản lưu trong vault (backup + push GitHub)
~/oskernel-lab/                 ← bản làm việc trong WSL, BUILD Ở ĐÂY
```

```bash
cd ~/oskernel-lab/03-pmm-vmm
make info      # đang dùng cross gcc hay gcc hệ thống
make           # kernel.bin + kernel.map/.dis/.sym/.elf.txt
make run       # QEMU headless, serial ra stdout
make run-gfx   # có cửa sổ (dùng cho Lab 0x02)
```

Dựng môi trường: [[Dựng môi trường oskernel-lab]] · lệnh chi tiết + cấu trúc `os.img`:
[[Cẩm nang build & debug]].

> **Sửa code xong nhớ chép ngược** về `Thực hành/src/oskernel-lab/` rồi commit — vault là thứ duy nhất
> được backup lên GitHub.

## Output thật đã lưu

- `outputs/lab-0x03-run.txt` — bản đầy đủ bảng `pmm_dump_map` (rộng ~176 cột, markdown không vừa)
- `outputs/lab-0x03-run-2026-09-03.txt` — lần chạy 2026-09-03, sau khi vá bug bitmap
- `outputs/cavos-boot-2026-09-03.txt` — boot log của **cavOS thật**, để đối chiếu với lab
