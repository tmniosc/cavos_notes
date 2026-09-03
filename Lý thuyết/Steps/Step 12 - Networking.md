---
tags: [step, network]
status: todo
---

# Step 12 — Networking

**Hàm:** `initiateNetworking()` · **Phụ thuộc:** NIC từ [[Step 13 - PCI & NIC]]

## 🎯 Mục đích
**Giao tiếp mạng**: dựng stack tối thiểu Ethernet→ARP→IPv4→ICMP/UDP để máy gửi/nhận gói. Xây trên NIC do
[[Step 13 - PCI & NIC]] tìm và điều khiển. Cho phép ping/trao đổi dữ liệu với thế giới bên ngoài.

## Mục tiêu học
- Stack tối thiểu: Ethernet → ARP → IPv4 → ICMP/UDP.
- Buffer gói, hàng đợi TX/RX.

## Câu hỏi mở
- [ ] Thứ tự init: networking trước hay NIC trước? (xem [[Boot Flow (_start)]])
- [ ] Endianness mạng (big-endian) vs host.

## Ánh xạ spec
- RFC 791 (IP), 826 (ARP), 792 (ICMP).
