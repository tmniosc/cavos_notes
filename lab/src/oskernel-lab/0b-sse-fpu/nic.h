/* nic.h — lớp quản lý NIC, theo cavOS include/nic_controller.h + drivers/nics/nic_controller.c
 * (Bài 13 + 14).
 *
 * Giống cavOS: struct NIC chung cho mọi card (type, mtu, MAC, irq, infoLocation trỏ struct
 * riêng của driver), selectedNIC, initiateNetworking() / initiateNIC() / createNewNIC(),
 * sendPacket() (tự thêm header Ethernet) / sendPacketRaw(), hàng đợi netQueue[128] mà
 * handler ngắt đổ frame vào và thread helper lấy ra (helperNet -> handlePacket).
 * Khác cavOS: không có lwIP. handlePacket() đưa frame cho net.c (lab tự giải mã
 * ARP/IPv4/UDP/DHCP/ICMP) thay vì nic->lwip.input = tcpip_input.
 */
#pragma once
#include <stdint.h>
#include "pci.h"

typedef enum { NE2000, RTL8139, RTL8169, E1000 } NIC_TYPE;

typedef struct NIC NIC;
struct NIC {
    NIC_TYPE type;
    uint16_t mtu;
    uint8_t  mintu;
    void    *infoLocation;          /* E1000_interface* */
    uint8_t  MAC[6];
    uint8_t  ip[4];                 /* cavOS: lwIP giữ; lab: net.c điền sau DHCP ACK */
    uint8_t  irq;                   /* = interruptLine trong config space (như cavOS) */
};

extern NIC *selectedNIC;

/* frame Ethernet II: 14 byte header */
typedef struct {
    uint8_t  destination_mac[6];
    uint8_t  source_mac[6];
    uint16_t ethertype;             /* big-endian trên dây */
} __attribute__((packed)) netPacketHeader;

enum { NET_ETHERTYPE_ARP = 0x0806, NET_ETHERTYPE_IPV4 = 0x0800, NET_ETHERTYPE_IPV6 = 0x86DD };

void initiateNetworking(void);
void initiateNIC(PCIdevice *device);
NIC *createNewNIC(PCI *pci);

void sendPacket(NIC *nic, const uint8_t *destination_mac, const void *data, uint32_t size,
                uint16_t protocol);
void sendPacketRaw(NIC *nic, const void *data, uint32_t size);
void handlePacket(NIC *nic, void *packet, uint32_t size);

/* netQueue: IRQ (ghi) -> helper thread (đọc). Một người ghi, một người đọc. */
#define PACKET_MAX 1600
#define QUEUE_MAX  128
typedef struct {
    NIC     *nic;
    uint8_t  buff[PACKET_MAX];
    uint16_t packetLength;
    uint64_t irqTick;               /* lab: tick lúc IRQ chép frame vào hàng đợi */
} QueuePacket;

extern QueuePacket  netQueue[QUEUE_MAX];
extern volatile int netQueueRead, netQueueWrite;
extern volatile uint64_t netQueueDropped;

void netQueueAdd(NIC *nic, uint8_t *packet, uint16_t packetLength);
void helperNet(void);               /* = cavOS kernel_helper.c helperNet() */

static inline uint16_t switch_endian_16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
