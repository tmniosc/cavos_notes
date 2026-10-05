/* e1000.h — Intel 8254x (QEMU "e1000" = 82540EM, 8086:100e), theo cavOS include/e1000.h.
 * Tên thanh ghi/bit giữ như cavOS; số offset đối chiếu "PCI/PCI-X Family of Gigabit Ethernet
 * Controllers Software Developer's Manual" (8254x SDM) bảng 13-2.
 */
#pragma once
#include <stdint.h>
#include "nic.h"
#include "pci.h"

/* 1 trang mỗi ring như cavOS: 4096 / 16 = 256 descriptor */
#define E1000_RX_LIST_ENTRIES 256
#define E1000_TX_LIST_ENTRIES 256
#define E1000_RX_BUFFER_SIZE  2048   /* RCTL.BSIZE = 00, BSEX = 0 */

/* Legacy receive descriptor (8254x SDM §3.2.3) — 16 byte, card GHI vào */
typedef struct {
    uint64_t addr;                  /* PA buffer */
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;                /* bit 0 DD (descriptor done), bit 1 EOP */
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) E1000RX;

/* Legacy transmit descriptor (8254x SDM §3.3.3) — 16 byte, driver ghi, card trả DD */
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  checksumOffset;
    uint8_t  command;               /* EOP | IFCS | RS */
    uint8_t  status;                /* bit 0 DD */
    uint8_t  checksumStart;
    uint16_t special;
} __attribute__((packed)) E1000TX;

#define E1000RX_STATUS_DONE          (1 << 0)
#define E1000RX_STATUS_END_OF_PACKET (1 << 1)

typedef struct {
    uint64_t membasePhys;
    uint64_t membase;               /* VA uncached (lab) — cavOS: hhdmOffset + phys */
    uint64_t mmioSize;
    uint16_t deviceId;
    NIC     *nic;

    volatile E1000RX *rxList;
    uint64_t rxListPhys;
    uint32_t rxHead;                /* descriptor kế tiếp driver sẽ đọc */
    uint8_t *rxBuf[E1000_RX_LIST_ENTRIES];

    volatile E1000TX *txList;
    uint64_t txListPhys;
    uint8_t *txBuf[E1000_TX_LIST_ENTRIES];
    uint64_t txBufPhys[E1000_TX_LIST_ENTRIES];

    int      eeprom;
    uint8_t  gsi, vector;
} E1000_interface;

/* thanh ghi */
#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EECD        0x0010
#define REG_EEPROM      0x0014      /* EERD */
#define REG_ICR         0x00C0      /* đọc = lấy + xoá nguyên nhân ngắt */
#define REG_ICS         0x00C8      /* ghi = tự gây ngắt (lab dùng để dò GSI) */
#define REG_IMASK       0x00D0      /* IMS */
#define REG_IMASK_CLEAR 0x00D8      /* IMC */
#define REG_RX_CONTROL  0x0100      /* RCTL */
#define REG_TCTL        0x0400
#define REG_RXDESCLO    0x2800
#define REG_RXDESCHI    0x2804
#define REG_RXDESCLEN   0x2808
#define REG_RXDESCHEAD  0x2810
#define REG_RXDESCTAIL  0x2818
#define REG_TXDESCLO    0x3800
#define REG_TXDESCHI    0x3804
#define REG_TXDESCLEN   0x3808
#define REG_TXDESCHEAD  0x3810
#define REG_TXDESCTAIL  0x3818
#define REG_RAL_BEGIN   0x5400
#define REG_RAH         0x5404

/* CTRL */
#define CTRL_LINK_RESET            (1u << 3)
#define CTRL_SET_LINK_UP           (1u << 6)
#define CTRL_INVERT_LOSS_OF_SIGNAL (1u << 7)
#define CTRL_DEVICE_RESET          (1u << 26)
#define CTRL_VLAN_MODE_ENABLE      (1u << 30)
#define CTRL_PHY_RESET             (1u << 31)

/* EEPROM */
#define EECD_EEPROM_PRESENT (1 << 8)
#define EERD_START          (1 << 0)
#define EERD_DONE           (1 << 4)

/* RCTL — y cavOS */
#define RCTL_ENABLE                        (1 << 1)
#define RCTL_STORE_BAD_PACKETS             (1 << 2)
#define RCTL_UNICAST_PROMISCUOUS_ENABLED   (1 << 3)
#define RCTL_MULTICAST_PROMISCUOUS_ENABLED (1 << 4)
#define RCTL_LONG_PACKET_RECEPTION_ENABLE  (1 << 5)
#define RCTL_LOOPBACK_MODE_OFF             (0b00 << 6)
#define RCTL_DESC_MIN_THRESHOLD_SIZE_HALF  (0b00 << 8)
#define RCTL_BROADCAST_ACCEPT_MODE         (1 << 15)
#define RCTL_BUFFER_SIZE_2048              (0b00 << 16)
#define RCTL_STRIP_ETHERNET_CRC            (1 << 26)
/* cavOS viết "& (1 << 25)" thay vì "| (1 << 25)" -> cả biểu thức bằng 0 (= cỡ 2048) */
#define CAVOS_RCTL_BUFFER_SIZE_8192        ((0b10 << 16) & (1 << 25))

/* TCTL */
#define TCTL_EN  (1 << 1)
#define TCTL_PSP (1 << 3)           /* pad short packets lên 64 byte (lab thêm) */

/* ICR / IMS — cùng vị trí bit */
#define ICR_TX_DESC_WRITTEN_BACK      (1 << 0)
#define ICR_TX_QUEUE_EMPTY            (1 << 1)
#define ICR_LINK_STATUS_CHANGE        (1 << 2)
#define ICR_RX_SEQUENCE_ERROR         (1 << 3)
#define ICR_RX_DESC_MIN_THRESHOLD_HIT (1 << 4)
#define ICR_RX_OVERRUN                (1 << 6)
#define ICR_RX_TIMER_INTERRUPT        (1 << 7)
#define ICR_TX_DESC_MIN_THRESHOLD_HIT (1 << 15)

/* TX command */
#define CMD_EOP  (1 << 0)
#define CMD_IFCS (1 << 1)
#define CMD_RS   (1 << 3)

/* đếm số liệu (lab) */
typedef struct {
    volatile uint64_t irqs;          /* số lần handler chạy */
    volatile uint64_t irqsEmpty;     /* ICR == 0: không có nguyên nhân nào */
    volatile uint64_t txdw, txqe, lsc, rxdmt, rxo, rxt0;
    volatile uint64_t rxFrames, txFrames;
    volatile uint64_t lastRxTick;
} E1000stats;
extern E1000stats e1000Stats;
extern uint64_t   e1000RctlTick;

/* ảnh chụp descriptor TX vừa gửi (lab in ra) */
typedef struct {
    uint32_t index, tdhBefore, tdhAfter, tdtAfter;
    uint64_t addr;
    uint16_t length;
    uint8_t  command, status;
    uint64_t waitTicks, yields;
} E1000lastTx;
extern E1000lastTx e1000LastTx;

int  initiateE1000(PCIdevice *device);
void sendE1000(NIC *nic, const void *packet, uint32_t packetSize);
void e1000_dump_rings(const char *when);
const uint8_t *e1000_tx_buffer(uint32_t index);   /* VA của buffer TX (lab in hexdump) */
uint32_t E1000CmdRead(E1000_interface *e1000, uint16_t addr);
