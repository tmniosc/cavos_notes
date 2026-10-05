#!/usr/bin/env python3
"""pcap_summary.py — đọc file pcap do QEMU '-object filter-dump' ghi ra, in mỗi frame một dòng.

Không cần tcpdump/wireshark: định dạng pcap cổ điển chỉ là header 24 byte + mỗi gói
(16 byte header: ts_sec, ts_usec, incl_len, orig_len) + dữ liệu. filter-dump ghi
linktype 1 (Ethernet). Giải mã đủ cho lab: ARP, IPv4 (UDP/DHCP, ICMP), IPv6 (chỉ tên).

    python3 scripts/pcap_summary.py lab08.pcap
"""
import struct
import sys

def mac(b):
    return ":".join(f"{x:02x}" for x in b)

def ip4(b):
    return ".".join(str(x) for x in b)

DHCP_TYPES = {1: "DISCOVER", 2: "OFFER", 3: "REQUEST", 4: "DECLINE", 5: "ACK", 6: "NAK", 7: "RELEASE", 8: "INFORM"}

def dhcp(p):
    if len(p) < 240 or p[236:240] != b"\x63\x82\x53\x63":
        return "BOOTP (no magic cookie)"
    op, xid = p[0], struct.unpack(">I", p[4:8])[0]
    yiaddr = ip4(p[16:20])
    opts, i, out = p[240:], 0, {}
    while i < len(opts) and opts[i] != 255:
        if opts[i] == 0:
            i += 1
            continue
        code, ln = opts[i], opts[i + 1]
        out[code] = opts[i + 2:i + 2 + ln]
        i += 2 + ln
    kind = DHCP_TYPES.get(out.get(53, b"\0")[0], "?")
    s = f"DHCP {kind} xid=0x{xid:08x}"
    if op == 2:
        s += f" yiaddr={yiaddr}"
    if 54 in out:
        s += f" server={ip4(out[54])}"
    if 50 in out:
        s += f" requested={ip4(out[50])}"
    if 1 in out:
        s += f" mask={ip4(out[1])}"
    if 3 in out:
        s += f" router={ip4(out[3][:4])}"
    if 6 in out:
        s += f" dns={ip4(out[6][:4])}"
    if 51 in out:
        s += f" lease={struct.unpack('>I', out[51])[0]}s"
    return s

def decode(f):
    if len(f) < 14:
        return "runt"
    dst, src, et = f[0:6], f[6:12], struct.unpack(">H", f[12:14])[0]
    head = f"{mac(src)} > {mac(dst)}"
    p = f[14:]
    if et == 0x0806 and len(p) >= 28:
        op = struct.unpack(">H", p[6:8])[0]
        sha, spa, tpa = p[8:14], p[14:18], p[24:28]
        if op == 1:
            return f"{head} ARP request who-has {ip4(tpa)} tell {ip4(spa)}"
        return f"{head} ARP reply {ip4(spa)} is-at {mac(sha)}"
    if et == 0x0800 and len(p) >= 20:
        ihl = (p[0] & 0xF) * 4
        proto, s, d = p[9], ip4(p[12:16]), ip4(p[16:20])
        l4 = p[ihl:]
        if proto == 17 and len(l4) >= 8:
            sp, dp = struct.unpack(">HH", l4[0:4])
            extra = dhcp(l4[8:]) if {sp, dp} & {67, 68} else f"UDP {sp} > {dp}"
            return f"{head} IPv4 {s} > {d} {extra}"
        if proto == 1 and len(l4) >= 8:
            t, ident, seq = l4[0], *struct.unpack(">HH", l4[4:8])
            name = {0: "echo reply", 8: "echo request"}.get(t, f"type {t}")
            return f"{head} IPv4 {s} > {d} ICMP {name} id=0x{ident:04x} seq={seq}"
        return f"{head} IPv4 {s} > {d} proto {proto}"
    if et == 0x86DD:
        nh = p[6] if len(p) > 6 else -1
        icmp6 = ""
        if nh == 58 and len(p) > 40:
            icmp6 = {133: " ICMPv6 router solicitation", 134: " ICMPv6 router advertisement",
                     135: " ICMPv6 neighbor solicitation", 136: " ICMPv6 neighbor advertisement"}.get(p[40], f" ICMPv6 type {p[40]}")
        return f"{head} IPv6 next-header {nh}{icmp6}"
    return f"{head} ethertype 0x{et:04x}"

def main(path):
    data = open(path, "rb").read()
    magic = struct.unpack("<I", data[:4])[0]
    if magic not in (0xA1B2C3D4, 0xA1B23C4D):
        sys.exit(f"{path}: not a classic pcap file (magic 0x{magic:08x})")
    vmaj, vmin, _, _, snap, link = struct.unpack("<HHiIII", data[4:24])
    print(f"# {path}: pcap v{vmaj}.{vmin}, linktype {link} ({'Ethernet' if link == 1 else '?'}), snaplen {snap}")
    off, n, t0 = 24, 0, None
    while off + 16 <= len(data):
        sec, usec, incl, orig = struct.unpack("<IIII", data[off:off + 16])
        frame = data[off + 16:off + 16 + incl]
        off += 16 + incl
        n += 1
        t = sec + usec / 1e6
        t0 = t if t0 is None else t0
        print(f"{n:3d}  +{t - t0:8.6f}s  len {orig:4d}  {decode(frame)}")
    print(f"# {n} frames")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "lab08.pcap")
