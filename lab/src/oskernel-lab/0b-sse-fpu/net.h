/* net.h — "stack" tối thiểu của lab: dựng và đọc vài loại frame bằng tay (Bài 13).
 *
 * KHÔNG phải TCP/IP stack. cavOS dùng lwIP (networking/lwip/): ARP cache, IPv4, phân mảnh,
 * UDP, TCP, DHCP client, DNS, socket. Lab chỉ làm đủ để chứng minh driver gửi và nhận được
 * frame thật: DHCP DISCOVER/REQUEST (giống dhcp_start() của lwIP trong cavOS), ARP request,
 * ICMP echo. Không có TCP, không có bảng định tuyến, không retransmit, một NIC.
 */
#pragma once
#include <stdint.h>
#include "nic.h"

void net_input(NIC *nic, uint8_t *frame, uint32_t len);   /* gọi từ handlePacket() */

int  net_dhcp(NIC *nic);                                  /* 1 = đã có ACK, nic->ip đã điền */
int  net_arp_resolve(NIC *nic, const uint8_t ip[4], uint8_t mac[6]);
int  net_ping(NIC *nic, const uint8_t ip[4], const uint8_t mac[6], uint16_t seq);

extern uint8_t netRouter[4], netDns[4], netMask[4], netServer[4];
extern uint32_t netLease;
extern volatile uint64_t netRxCount, netRxIgnored;
