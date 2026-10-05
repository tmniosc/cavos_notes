/* net.c — dựng/đọc frame ARP, IPv4, UDP/DHCP, ICMP bằng tay (Bài 13). Xem net.h.
 * Mọi số nhiều byte trên dây là big-endian ("network byte order"); x86 là little-endian,
 * nên mỗi trường 16/32 bit đều phải đảo (be16/be32).
 */
#include "net.h"
#include "boot.h"
#include "e1000.h"
#include "kout.h"
#include "serial.h"
#include "task.h"
#include "timer.h"

uint8_t  netRouter[4], netDns[4], netMask[4], netServer[4];
uint32_t netLease;
volatile uint64_t netRxCount, netRxIgnored;
extern uint64_t netCurrentIrqTick;   /* nic.c */

static const uint8_t macBroadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t ipZero[4] = {0, 0, 0, 0};
static const uint8_t ipBroadcast[4] = {255, 255, 255, 255};
#define DHCP_XID 0x4c414238u          /* "LAB8" */
#define PING_ID  0x0808

static inline uint16_t be16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static inline uint32_t be32(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
}
static int eq(const uint8_t *a, const uint8_t *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

/* ----------------------------------------------------------------- in */
static void putMac(const uint8_t *m) {
    const char *h = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        serial_putc(h[m[i] >> 4]);
        serial_putc(h[m[i] & 15]);
        if (i < 5) serial_putc(':');
    }
}
static void putIp(const uint8_t *ip) {
    for (int i = 0; i < 4; i++) {
        serial_putdec(ip[i]);
        if (i < 3) serial_putc('.');
    }
}
static void hexdump(const uint8_t *p, uint32_t n) {
    const char *h = "0123456789abcdef";
    for (uint32_t i = 0; i < n; i += 16) {
        serial_puts("      ");
        for (uint32_t j = i; j < i + 16 && j < n; j++) {
            serial_putc(h[p[j] >> 4]);
            serial_putc(h[p[j] & 15]);
            serial_putc(' ');
        }
        serial_putc('\n');
    }
}

/* -------------------------------------------------------- checksum IPv4 */
/* RFC 1071: cộng các từ 16 bit (bù 1), gập phần nhớ, đảo bit */
static uint16_t ipChecksum(const void *data, uint32_t len) {
    const uint8_t *p = data;
    uint32_t sum = 0;
    for (uint32_t i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)(p[i] << 8 | p[i + 1]);
    if (len & 1)
        sum += (uint32_t)p[len - 1] << 8;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return be16((uint16_t)~sum);              /* trả theo thứ tự byte trên dây */
}

/* ------------------------------------------------------------ cấu trúc */
typedef struct {
    uint16_t htype, ptype;
    uint8_t  hlen, plen;
    uint16_t op;
    uint8_t  sha[6], spa[4], tha[6], tpa[4];
} __attribute__((packed)) ArpPacket;

typedef struct {
    uint8_t  verIhl, tos;
    uint16_t totalLen, id, flagsFrag;
    uint8_t  ttl, proto;
    uint16_t csum;
    uint8_t  src[4], dst[4];
} __attribute__((packed)) Ipv4Header;

typedef struct { uint16_t sport, dport, len, csum; } __attribute__((packed)) UdpHeader;

typedef struct {
    uint8_t  op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint8_t  ciaddr[4], yiaddr[4], siaddr[4], giaddr[4];
    uint8_t  chaddr[16];
    uint8_t  sname[64], file[128];
    uint32_t cookie;                          /* 0x63825363 */
    uint8_t  options[60];                     /* BOOTP tối thiểu 300 byte */
} __attribute__((packed)) DhcpPacket;

typedef struct {
    uint8_t  type, code;
    uint16_t csum, id, seq;
} __attribute__((packed)) IcmpHeader;

/* ------------------------------------------------- giải mã 1 frame để in */
static const char *dhcpName(int t) {
    static const char *n[] = {"?", "DISCOVER", "OFFER", "REQUEST", "DECLINE", "ACK", "NAK", "RELEASE"};
    return (t >= 1 && t <= 7) ? n[t] : "?";
}

/* đọc option DHCP: trả con trỏ tới dữ liệu, *len = độ dài */
static const uint8_t *dhcpOpt(const DhcpPacket *d, uint32_t avail, uint8_t code, int *len) {
    const uint8_t *o = d->options, *end = (const uint8_t *)d + avail;
    while (o < end && *o != 255) {
        if (*o == 0) { o++; continue; }
        if (o + 2 > end) break;
        if (*o == code) {
            *len = o[1];
            return o + 2;
        }
        o += 2 + o[1];
    }
    return 0;
}

static void describe(const uint8_t *f, uint32_t len) {
    const netPacketHeader *eth = (const netPacketHeader *)f;
    putMac(eth->source_mac);
    serial_puts(" > ");
    putMac(eth->destination_mac);
    uint16_t et = be16(eth->ethertype);
    const uint8_t *p = f + 14;
    if (et == NET_ETHERTYPE_ARP) {
        const ArpPacket *a = (const ArpPacket *)p;
        if (be16(a->op) == 1) {
            serial_puts(" ARP request who-has ");
            putIp(a->tpa);
            serial_puts(" tell ");
            putIp(a->spa);
        } else {
            serial_puts(" ARP reply ");
            putIp(a->spa);
            serial_puts(" is-at ");
            putMac(a->sha);
        }
    } else if (et == NET_ETHERTYPE_IPV4) {
        const Ipv4Header *ip = (const Ipv4Header *)p;
        uint32_t ihl = (ip->verIhl & 0xf) * 4;
        serial_puts(" IPv4 ");
        putIp(ip->src);
        serial_puts(" > ");
        putIp(ip->dst);
        const uint8_t *l4 = p + ihl;
        if (ip->proto == 17) {
            const UdpHeader *u = (const UdpHeader *)l4;
            serial_puts(" UDP ");
            serial_putdec(be16(u->sport));
            serial_puts(" > ");
            serial_putdec(be16(u->dport));
            if (be16(u->sport) == 67 || be16(u->sport) == 68) {
                const DhcpPacket *d = (const DhcpPacket *)(l4 + 8);
                uint32_t avail = len - (uint32_t)((const uint8_t *)d - f);
                int ol = 0;
                const uint8_t *t = dhcpOpt(d, avail, 53, &ol);
                serial_puts(" DHCP ");
                serial_puts(t ? dhcpName(t[0]) : "?");
                if (d->op == 2) {
                    serial_puts(" yiaddr=");
                    putIp(d->yiaddr);
                }
            }
        } else if (ip->proto == 1) {
            const IcmpHeader *ic = (const IcmpHeader *)l4;
            serial_puts(ic->type == 8 ? " ICMP echo request" : ic->type == 0 ? " ICMP echo reply" : " ICMP");
            serial_puts(" seq=");
            serial_putdec(be16(ic->seq));
        } else {
            serial_puts(" proto ");
            serial_putdec(ip->proto);
        }
    } else if (et == NET_ETHERTYPE_IPV6) {
        serial_puts(" IPv6 (lab ignores it)");
    } else {
        serial_puts(" ethertype ");
        serial_puthex_short(et);
    }
}

/* ------------------------------------------------------------------ gửi */
static uint8_t txFrame[1600];
static int     txHexdumps;              /* hexdump vài frame đầu */

static void sendFrame(NIC *nic, const uint8_t *dst, uint16_t ethertype, const void *payload,
                      uint32_t size, const char *what) {
    netPacketHeader *h = (netPacketHeader *)txFrame;
    memcpy(h->destination_mac, dst, 6);
    memcpy(h->source_mac, nic->MAC, 6);
    h->ethertype = be16(ethertype);
    memcpy(txFrame + 14, payload, size);

    kout_begin();
    serial_puts("tx ");
    serial_puts(what);
    serial_puts(" (");
    serial_putdec(14 + size);
    serial_puts(" B): ");
    describe(txFrame, 14 + size);
    serial_putc('\n');
    kout_end();

    sendPacket(nic, dst, payload, size, ethertype);   /* = cavOS sendPacket -> sendE1000 */

    kout_begin();
    serial_puts("   TX desc #");
    serial_putdec(e1000LastTx.index);
    serial_puts(": addr=");
    serial_puthex_short(e1000LastTx.addr);
    serial_puts(" len=");
    serial_putdec(e1000LastTx.length);
    serial_puts(" cmd=");
    serial_puthex_short(e1000LastTx.command);
    serial_puts(" (EOP|IFCS|RS) status=");
    serial_puthex_short(e1000LastTx.status);
    serial_puts(" (DD)  TDT -> ");
    serial_putdec(e1000LastTx.tdtAfter);
    serial_puts(", TDH ");
    serial_putdec(e1000LastTx.tdhBefore);
    serial_puts(" -> ");
    serial_putdec(e1000LastTx.tdhAfter);
    serial_puts(", DD after ");
    serial_putdec(e1000LastTx.waitTicks);
    serial_puts(" ms / ");
    serial_putdec(e1000LastTx.yields);
    serial_puts(" handControl()\n");
    if (txHexdumps < 1) {
        txHexdumps++;
        serial_puts("   TX buffer bytes (the card fetches these by DMA from PA addr):\n");
        hexdump(e1000_tx_buffer(e1000LastTx.index), e1000LastTx.length);
    }
    kout_end();
}

static void ipv4Fill(Ipv4Header *ip, uint8_t proto, const uint8_t *src, const uint8_t *dst,
                     uint16_t payloadLen) {
    static uint16_t ident = 1;
    memset(ip, 0, sizeof(*ip));
    ip->verIhl = 0x45;                       /* IPv4, 5 x 4 = 20 byte header */
    ip->totalLen = be16((uint16_t)(sizeof(Ipv4Header) + payloadLen));
    ip->id = be16(ident++);
    ip->ttl = 64;
    ip->proto = proto;
    memcpy(ip->src, src, 4);
    memcpy(ip->dst, dst, 4);
    ip->csum = 0;
    ip->csum = ipChecksum(ip, sizeof(*ip));
}

/* ---------------------------------------------------------- trạng thái */
static volatile int  dhcpGot;               /* loại DHCP vừa nhận (2 OFFER, 5 ACK) */
static uint8_t       offerIp[4];
static volatile int  arpGot;
static uint8_t       arpWantIp[4], arpMac[6];
static volatile int  pingGot;
static volatile uint16_t pingSeqGot;
static volatile uint64_t gotTick, gotIrqTick;
static int           rxHexdumps;

static int waitFor(volatile int *flag, uint64_t ms) {
    uint64_t until = timerTicks + ms;
    while (!*flag && timerTicks < until)
        taskSleepMs(1);
    return *flag;
}

/* ------------------------------------------------------------- nhận */
static void handleDhcp(NIC *nic, const DhcpPacket *d, uint32_t avail) {
    (void)nic;
    if (d->op != 2 || be32(d->xid) != DHCP_XID || d->cookie != be32(0x63825363))
        return;
    int ol;
    const uint8_t *t = dhcpOpt(d, avail, 53, &ol);
    if (!t)
        return;
    const uint8_t *v;
    kout_begin_raw();
    serial_puts("   DHCP ");
    serial_puts(dhcpName(t[0]));
    serial_puts(": your IP ");
    putIp(d->yiaddr);
    if ((v = dhcpOpt(d, avail, 54, &ol))) { serial_puts(", server "); putIp(v); memcpy(netServer, v, 4); }
    if ((v = dhcpOpt(d, avail, 1, &ol)))  { serial_puts(", mask "); putIp(v); memcpy(netMask, v, 4); }
    if ((v = dhcpOpt(d, avail, 3, &ol)))  { serial_puts(", router "); putIp(v); memcpy(netRouter, v, 4); }
    if ((v = dhcpOpt(d, avail, 6, &ol)))  { serial_puts(", DNS "); putIp(v); memcpy(netDns, v, 4); }
    if ((v = dhcpOpt(d, avail, 51, &ol))) {
        netLease = ((uint32_t)v[0] << 24) | (v[1] << 16) | (v[2] << 8) | v[3];
        serial_puts(", lease ");
        serial_putdec(netLease);
        serial_puts(" s");
    }
    serial_putc('\n');
    kout_end();
    memcpy(offerIp, d->yiaddr, 4);
    gotTick = timerTicks;
    gotIrqTick = netCurrentIrqTick;
    dhcpGot = t[0];
}

static void handleArp(NIC *nic, const ArpPacket *a) {
    uint16_t op = be16(a->op);
    if (op == 2 && eq(a->spa, arpWantIp, 4)) {
        memcpy(arpMac, a->sha, 6);
        gotTick = timerTicks;
        gotIrqTick = netCurrentIrqTick;
        arpGot = 1;
    } else if (op == 1 && eq(a->tpa, nic->ip, 4) && !eq(nic->ip, ipZero, 4)) {
        /* ai đó hỏi IP của mình: trả lời (lwIP etharp làm việc này trong cavOS) */
        ArpPacket r;
        r.htype = be16(1);
        r.ptype = be16(0x0800);
        r.hlen = 6;
        r.plen = 4;
        r.op = be16(2);
        memcpy(r.sha, nic->MAC, 6);
        memcpy(r.spa, nic->ip, 4);
        memcpy(r.tha, a->sha, 6);
        memcpy(r.tpa, a->spa, 4);
        sendFrame(nic, a->sha, NET_ETHERTYPE_ARP, &r, sizeof(r), "ARP reply");
    }
}

/* = điểm vào từ handlePacket(): trong cavOS đây là chỗ frame đi vào lwIP */
void net_input(NIC *nic, uint8_t *f, uint32_t len) {
    netRxCount++;
    kout_begin();
    serial_puts("rx (");
    serial_putdec(len);
    serial_puts(" B, IRQ at t=");
    serial_putdec(netCurrentIrqTick);
    serial_puts("): ");
    describe(f, len);
    serial_putc('\n');
    if (rxHexdumps < 1 && be16(((netPacketHeader *)f)->ethertype) == NET_ETHERTYPE_ARP) {
        rxHexdumps++;
        serial_puts("   frame bytes as the card wrote them by DMA (copied to netQueue by the IRQ):\n");
        hexdump(f, len);
    }
    kout_end();

    netPacketHeader *eth = (netPacketHeader *)f;
    uint16_t et = be16(eth->ethertype);
    uint8_t *p = f + 14;
    if (et == NET_ETHERTYPE_ARP) {
        handleArp(nic, (ArpPacket *)p);
    } else if (et == NET_ETHERTYPE_IPV4) {
        Ipv4Header *ip = (Ipv4Header *)p;
        uint32_t ihl = (ip->verIhl & 0xf) * 4;
        if (ipChecksum(ip, ihl) != 0) {          /* tổng cả header (gồm csum) phải ra 0 */
            netRxIgnored++;
            return;
        }
        uint8_t *l4 = p + ihl;
        if (ip->proto == 17 && be16(((UdpHeader *)l4)->dport) == 68) {
            handleDhcp(nic, (DhcpPacket *)(l4 + 8), len - (uint32_t)(l4 + 8 - f));
        } else if (ip->proto == 1) {
            IcmpHeader *ic = (IcmpHeader *)l4;
            if (ic->type == 0 && be16(ic->id) == PING_ID) {
                pingSeqGot = be16(ic->seq);
                gotTick = timerTicks;
                gotIrqTick = netCurrentIrqTick;
                pingGot = 1;
            }
        } else {
            netRxIgnored++;
        }
    } else {
        netRxIgnored++;
    }
}

/* ----------------------------------------------------------------- DHCP */
/* Một gói DHCP = Ethernet + IPv4 (0.0.0.0 -> 255.255.255.255) + UDP 68 -> 67 + BOOTP */
static void dhcpSend(NIC *nic, uint8_t type) {
    static uint8_t buf[sizeof(Ipv4Header) + sizeof(UdpHeader) + sizeof(DhcpPacket)];
    memset(buf, 0, sizeof(buf));
    Ipv4Header *ip = (Ipv4Header *)buf;
    UdpHeader  *udp = (UdpHeader *)(ip + 1);
    DhcpPacket *d = (DhcpPacket *)(udp + 1);

    d->op = 1;                                  /* BOOTREQUEST */
    d->htype = 1;                               /* Ethernet */
    d->hlen = 6;
    d->xid = be32(DHCP_XID);
    d->flags = be16(0x8000);                    /* xin server trả lời broadcast */
    memcpy(d->chaddr, nic->MAC, 6);
    d->cookie = be32(0x63825363);
    uint8_t *o = d->options;
    *o++ = 53; *o++ = 1; *o++ = type;           /* DHCP message type */
    if (type == 3) {                            /* REQUEST: IP muốn lấy + server đã chọn */
        *o++ = 50; *o++ = 4; memcpy(o, offerIp, 4); o += 4;
        *o++ = 54; *o++ = 4; memcpy(o, netServer, 4); o += 4;
    }
    *o++ = 55; *o++ = 3; *o++ = 1; *o++ = 3; *o++ = 6;   /* xin mask, router, DNS */
    *o++ = 255;

    udp->sport = be16(68);
    udp->dport = be16(67);
    udp->len = be16(sizeof(UdpHeader) + sizeof(DhcpPacket));
    udp->csum = 0;                              /* IPv4 cho phép bỏ checksum UDP */
    ipv4Fill(ip, 17, ipZero, ipBroadcast, sizeof(UdpHeader) + sizeof(DhcpPacket));
    sendFrame(nic, macBroadcast, NET_ETHERTYPE_IPV4, buf, sizeof(buf),
              type == 1 ? "DHCP DISCOVER" : "DHCP REQUEST");
}

static void gotLine(const char *what, uint64_t sent) {
    kout_begin();
    serial_puts(what);
    serial_puts(": sent at t=");
    serial_putdec(sent);
    serial_puts(", IRQ at t=");
    serial_putdec(gotIrqTick);
    serial_puts(", handled by helper at t=");
    serial_putdec(gotTick);
    serial_putc('\n');
    kout_end();
}

int net_dhcp(NIC *nic) {
    dhcpGot = 0;
    uint64_t t0 = timerTicks;
    dhcpSend(nic, 1);
    if (!waitFor(&dhcpGot, 2000) || dhcpGot != 2)
        return 0;
    gotLine("OFFER", t0);
    dhcpGot = 0;
    t0 = timerTicks;
    dhcpSend(nic, 3);
    if (!waitFor(&dhcpGot, 2000) || dhcpGot != 5)
        return 0;
    gotLine("ACK", t0);
    memcpy(nic->ip, offerIp, 4);
    return 1;
}

/* ------------------------------------------------------------------ ARP */
int net_arp_resolve(NIC *nic, const uint8_t ip[4], uint8_t mac[6]) {
    ArpPacket a;
    a.htype = be16(1);
    a.ptype = be16(0x0800);
    a.hlen = 6;
    a.plen = 4;
    a.op = be16(1);
    memcpy(a.sha, nic->MAC, 6);
    memcpy(a.spa, nic->ip, 4);
    memset(a.tha, 0, 6);
    memcpy(a.tpa, ip, 4);
    memcpy(arpWantIp, ip, 4);
    arpGot = 0;
    uint64_t t0 = timerTicks;
    sendFrame(nic, macBroadcast, NET_ETHERTYPE_ARP, &a, sizeof(a), "ARP request");
    if (!waitFor(&arpGot, 2000))
        return 0;
    gotLine("ARP reply", t0);
    memcpy(mac, arpMac, 6);
    return 1;
}

/* ----------------------------------------------------------------- ping */
int net_ping(NIC *nic, const uint8_t ip[4], const uint8_t mac[6], uint16_t seq) {
    static const char msg[] = "oskernel-lab 0x08 ping payload!";
    static uint8_t buf[sizeof(Ipv4Header) + sizeof(IcmpHeader) + sizeof(msg)];
    Ipv4Header *iph = (Ipv4Header *)buf;
    IcmpHeader *ic = (IcmpHeader *)(iph + 1);
    memset(buf, 0, sizeof(buf));
    ic->type = 8;
    ic->id = be16(PING_ID);
    ic->seq = be16(seq);
    memcpy(ic + 1, msg, sizeof(msg));
    ic->csum = 0;
    ic->csum = ipChecksum(ic, sizeof(IcmpHeader) + sizeof(msg));
    ipv4Fill(iph, 1, nic->ip, ip, sizeof(IcmpHeader) + sizeof(msg));
    pingGot = 0;
    uint64_t t0 = timerTicks;
    sendFrame(nic, mac, NET_ETHERTYPE_IPV4, buf, sizeof(buf), "ICMP echo request");
    if (!waitFor(&pingGot, 2000) || pingSeqGot != seq)
        return 0;
    gotLine("echo reply", t0);
    return 1;
}
