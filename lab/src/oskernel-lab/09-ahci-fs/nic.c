/* nic.c — = cavOS drivers/nics/nic_controller.c + helperNet() của entry/kernel_helper.c.
 *
 * Đường đi của một frame trong cavOS:
 *   nhận:  IRQ -> E1000InterruptHandler -> netQueueAdd (chép vào netQueue)
 *          -> thread helper: helperNet -> handlePacket -> pbuf -> nic->lwip.input (tcpip_input)
 *   gửi:   lwIP etharp_output -> netif->linkoutput = lwipOutput -> sendPacketRaw -> sendE1000
 * Lab giữ y phần NIC; chỗ lwIP thay bằng net_input() / net_send_*() trong net.c.
 */
#include "nic.h"
#include "e1000.h"
#include "boot.h"
#include "net.h"
#include "serial.h"
#include "timer.h"

NIC *selectedNIC;

static NIC nicPool[2];             /* cavOS: malloc(sizeof(NIC)) */
static int nicCount;

/* = cavOS initiateNetworking(): chưa có NIC nào. Không chạm phần cứng, nên gọi được
 * trước initiatePCI(); và PHẢI gọi trước, vì createNewNIC() (chạy trong initiatePCI)
 * mới là chỗ đặt selectedNIC — gọi sau thì nó xoá NIC vừa tìm thấy về 0. */
void initiateNetworking(void) {
    selectedNIC = 0;
    serial_puts("[networking] Ready to scan for NICs..\n");
}

/* = cavOS initiateNIC(): thử lần lượt từng driver, driver nào nhận thì thôi.
 * cavOS: initiateNe2000 || initiateRTL8139 || initiateRTL8169 || initiateE1000,
 * rồi tcpip_init(lwipInitInThread, selectedNIC) dựng lwIP + DHCP.
 * Lab chỉ có driver E1000 và không có lwIP: demo DHCP nằm ở thread "net" (kernel.c). */
void initiateNIC(PCIdevice *device) {
    if (initiateE1000(device)) {
        serial_puts("[nics] driver took the card; cavOS would now call tcpip_init(lwipInitInThread,"
                    " selectedNIC) -> lwIP thread + dhcp_start()\n");
    } else {
        serial_puts("[nics] no lab driver for this card (cavOS also tries ne2k, rtl8139, rtl8169)\n");
    }
}

/* = cavOS createNewNIC(): struct NIC rỗng, mtu 1500, gắn vào node PCI, thành selectedNIC */
NIC *createNewNIC(PCI *pci) {
    NIC *nic = &nicPool[nicCount < 2 ? nicCount++ : 1];
    memset(nic, 0, sizeof(NIC));
    nic->mtu = 1500;
    if (pci)
        pci->extra = nic;
    selectedNIC = nic;
    return nic;
}

/* = cavOS sendPacket(): ghép header Ethernet 14 byte trước dữ liệu. */
void sendPacket(NIC *nic, const uint8_t *destination_mac, const void *data, uint32_t size,
                uint16_t protocol) {
    static uint8_t frame[1600];    /* cavOS: malloc + free mỗi lần */
    if (size > nic->mtu) {
        serial_puts("[nics] packet larger than MTU, dropped\n");
        return;
    }
    netPacketHeader *h = (netPacketHeader *)frame;
    memcpy(h->source_mac, nic->MAC, 6);
    memcpy(h->destination_mac, destination_mac, 6);
    h->ethertype = switch_endian_16(protocol);
    memcpy(frame + sizeof(netPacketHeader), data, size);
    sendPacketRaw(nic, frame, sizeof(netPacketHeader) + size);
}

/* = cavOS sendPacketRaw(): chọn hàm gửi theo loại card.
 * cavOS so "size > nic->mtu" với size là CẢ frame (đã có 14 byte header), nên frame
 * 1501..1514 byte (gói IP đủ 1500) bị bỏ. Lab so với mtu + 14. */
void sendPacketRaw(NIC *nic, const void *data, uint32_t size) {
    if (size > (uint32_t)nic->mtu + sizeof(netPacketHeader)) {
        serial_puts("[nics] frame larger than MTU + 14, dropped\n");
        return;
    }
    switch (nic->type) {
    case E1000:
        sendE1000(nic, data, size);
        break;
    default:
        serial_puts("[nics] no send function for this NIC type\n");
        break;
    }
}

/* = cavOS handlePacket(): cavOS chép vào pbuf rồi nic->lwip.input(p, &nic->lwip). */
void handlePacket(NIC *nic, void *packet, uint32_t size) {
    net_input(nic, (uint8_t *)packet, size);
}

/* --------------------------------------------------------------- netQueue */
QueuePacket  netQueue[QUEUE_MAX];
volatile int netQueueRead, netQueueWrite;
volatile uint64_t netQueueDropped;

/* = cavOS netQueueAdd(): gọi từ handler ngắt. Ring đầy thì bỏ frame.
 * Khác cavOS: kiểm tra packetLength <= PACKET_MAX (cavOS memcpy thẳng; RX buffer
 * 2048 byte + LPE cho phép frame dài hơn 1600 -> tràn buff). */
void netQueueAdd(NIC *nic, uint8_t *packet, uint16_t packetLength) {
    if ((netQueueWrite + 1) % QUEUE_MAX == netQueueRead || packetLength > PACKET_MAX) {
        netQueueDropped++;
        return;
    }
    QueuePacket *item = &netQueue[netQueueWrite];
    item->nic = nic;
    memcpy(item->buff, packet, packetLength);
    item->packetLength = packetLength;
    item->irqTick = timerTicks;
    __atomic_store_n(&netQueueWrite, (netQueueWrite + 1) % QUEUE_MAX, __ATOMIC_RELEASE);
}

/* = cavOS helperNet(): rút hết hàng đợi, mỗi frame một lần handlePacket. Chạy trong thread
 * helper, tức ngữ cảnh task (được ngủ, được gửi gói trả lời), không phải ngữ cảnh ngắt. */
uint64_t netCurrentIrqTick;        /* lab: net.c in ra "IRQ at t=..." */
void helperNet(void) {
    while (netQueueRead != __atomic_load_n(&netQueueWrite, __ATOMIC_ACQUIRE)) {
        QueuePacket *q = &netQueue[netQueueRead];
        netCurrentIrqTick = q->irqTick;
        handlePacket(q->nic, q->buff, q->packetLength);
        netQueueRead = (netQueueRead + 1) % QUEUE_MAX;
    }
}
