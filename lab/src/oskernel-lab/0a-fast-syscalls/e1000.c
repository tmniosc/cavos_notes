/* e1000.c — driver Intel E1000 theo cavOS drivers/nics/e1000.c (Bài 14).
 *
 * Giống cavOS (cùng thứ tự trong initiateE1000): isE1000 (8086:100e QEMU, 8086:100f VMware)
 * -> bật bus master + memory space, xoá INTx disable -> createNewNIC -> BAR0 là MMIO
 * -> EEPROM (EECD bit 8) -> đọc MAC từ EERD word 0..2 -> CTRL: SLU=1, LRST/PHY_RST/VME/ILOS=0
 * -> RX ring 1 trang + RCTL -> TX ring 1 trang -> IRQ qua I/O APIC -> IMS -> đọc ICR
 * -> RCTL.EN, TCTL.EN. Gửi = ghi descriptor ở TDT, tăng TDT, chờ DD bằng handControl().
 *
 * Khác cavOS (có ghi trên trang lab, mục "Khác cavOS"):
 *  1. BAR0 map uncached vào VA riêng (cavOS: hhdmOffset + phys, như LAPIC ở Lab 0x05).
 *  2. Reset card (CTRL.RST) trước khi cấu hình — cavOS không reset.
 *  3. RX: RDT = N-1 và handler đi tiếp từ rxHead, trả từng descriptor bằng cách ghi RDT.
 *     cavOS ghi RDT = N (= 256, ngoài ring), không bao giờ dời RDT, và mỗi ngắt quét lại
 *     0..255 từ đầu (thứ tự frame sai khi ring quay vòng).
 *  4. TX: buffer cấp sẵn mỗi descriptor (cavOS cấp trang mới mỗi lần gửi, không trả).
 *  5. GSI của card: cavOS hỏi _PRT trong DSDT qua uACPI (AML). Lab không chạy AML nên DÒ:
 *     mở GSI 16..23, bảo card tự gây ngắt (ghi ICS), xem chân nào kêu.
 *  6. TCTL thêm PSP (đệm frame ngắn lên 64 byte), như 8254x SDM §14.5 khuyên.
 */
#include "e1000.h"
#include "apic.h"
#include "boot.h"
#include "isr.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "spinlock.h"
#include "task.h"
#include "timer.h"

E1000stats e1000Stats;
uint64_t   e1000RctlTick;           /* lab: tick lúc ghi RCTL lần cuối */
static E1000_interface e1000Info;         /* cavOS: malloc(sizeof(E1000_interface)) */
static Spinlock LOCK_E1000_TX;

/* ----------------------------------------------------------- thanh ghi */
uint32_t E1000CmdRead(E1000_interface *e, uint16_t addr) {
    return *(volatile uint32_t *)(e->membase + addr);
}
static void E1000CmdWrite(E1000_interface *e, uint16_t addr, uint32_t value) {
    *(volatile uint32_t *)(e->membase + addr) = value;
}

static int isE1000(PCIdevice *d) {
    return (d->vendor_id == 0x8086 && d->device_id == 0x100e) ||   /* qemu */
           (d->vendor_id == 0x8086 && d->device_id == 0x100f);     /* vmware */
}

static void macPrint(const uint8_t *m) {
    const char *h = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        serial_putc(h[m[i] >> 4]);
        serial_putc(h[m[i] & 15]);
        if (i < 5)
            serial_putc(':');
    }
}

/* -------------------------------------------------------------- EEPROM */
static void E1000EepromDetect(E1000_interface *e) {
    e->eeprom = (E1000CmdRead(e, REG_EECD) & EECD_EEPROM_PRESENT) != 0;
}

/* = cavOS E1000EepromRead (nhánh 82540: địa chỉ << 8, DONE = bit 4) */
static uint16_t E1000EepromRead(E1000_interface *e, uint8_t addr) {
    E1000CmdWrite(e, REG_EEPROM, EERD_START | ((uint32_t)addr << 8));
    uint32_t out;
    int spins = 0;
    do
        out = E1000CmdRead(e, REG_EEPROM);
    while (!(out & EERD_DONE) && ++spins < 1000000);   /* cavOS: chờ không giới hạn */
    return (uint16_t)(out >> 16);
}

/* ---------------------------------------------------------- RX / TX ring */
static uint64_t dmaFrame(void) {
    uint64_t pa = pmm_alloc();
    if (!pa) {
        serial_puts("[pci::e1000] out of memory\n");
        for (;;) __asm__ volatile("cli; hlt");
    }
    memset(P2V(pa), 0, PAGE_SIZE);
    return pa;
}

static void E1000RXConfigure(E1000_interface *e) {
    /* RAL/RAH: địa chỉ MAC card nhận. cavOS ghi 6 byte vào 0x5400 từng byte và không đặt
     * RAH.AV; lab ghi 2 thanh ghi 32 bit + AV (bit 31). (UPE bật nên card nhận hết dù sao.) */
    uint8_t *m = e->nic->MAC;
    E1000CmdWrite(e, REG_RAL_BEGIN, m[0] | (m[1] << 8) | (m[2] << 16) | ((uint32_t)m[3] << 24));
    E1000CmdWrite(e, REG_RAH, m[4] | (m[5] << 8) | (1u << 31));

    e->rxListPhys = dmaFrame();                          /* 256 x 16 B = 1 trang */
    e->rxList = (volatile E1000RX *)P2V(e->rxListPhys);
    uint64_t bufPa = 0;
    for (int i = 0; i < E1000_RX_LIST_ENTRIES; i++) {   /* 2 buffer 2 KiB mỗi frame 4 KiB */
        if (!(i & 1))
            bufPa = dmaFrame();
        uint64_t pa = bufPa + (i & 1) * E1000_RX_BUFFER_SIZE;
        e->rxList[i].addr = pa;
        e->rxBuf[i] = (uint8_t *)P2V(pa);
    }
    E1000CmdWrite(e, REG_RXDESCLO, (uint32_t)e->rxListPhys);
    E1000CmdWrite(e, REG_RXDESCHI, (uint32_t)(e->rxListPhys >> 32));
    E1000CmdWrite(e, REG_RXDESCLEN, E1000_RX_LIST_ENTRIES * sizeof(E1000RX));
    E1000CmdWrite(e, REG_RXDESCHEAD, 0);
    /* card sở hữu descriptor từ RDH tới RDT-1. RDT = N-1: đưa card 255 cái, chừa 1 cái
     * để "đầy" khác "rỗng". cavOS: RDT = 256 (ngoài ring). */
    E1000CmdWrite(e, REG_RXDESCTAIL, E1000_RX_LIST_ENTRIES - 1);
    e->rxHead = 0;

    E1000CmdWrite(e, REG_RX_CONTROL,
                  RCTL_LOOPBACK_MODE_OFF | RCTL_BROADCAST_ACCEPT_MODE |
                      RCTL_LONG_PACKET_RECEPTION_ENABLE | RCTL_UNICAST_PROMISCUOUS_ENABLED |
                      RCTL_MULTICAST_PROMISCUOUS_ENABLED | RCTL_DESC_MIN_THRESHOLD_SIZE_HALF |
                      RCTL_STRIP_ETHERNET_CRC | RCTL_STORE_BAD_PACKETS | RCTL_BUFFER_SIZE_2048);

    serial_puts("[pci::e1000] RX ring: 256 descriptors x 16 B = 1 page at PA ");
    serial_puthex_short(e->rxListPhys);
    serial_puts(", buffers 2048 B (2 per 4 KiB frame), RDH=");
    serial_putdec(E1000CmdRead(e, REG_RXDESCHEAD));
    serial_puts(" RDT=");
    serial_putdec(E1000CmdRead(e, REG_RXDESCTAIL));
    serial_puts("\n[pci::e1000] RCTL=");
    serial_puthex_short(E1000CmdRead(e, REG_RX_CONTROL));
    serial_puts(" (SBP UPE MPE LPE BAM SECRC, BSIZE=00 -> 2048 B); cavOS ORs in "
                "RCTL_BUFFER_SIZE_8192 = ((0b10 << 16) & (1 << 25)) = ");
    serial_puthex_short(CAVOS_RCTL_BUFFER_SIZE_8192);
    serial_puts(" -> also 2048 B\n");
}

static void E1000TXConfigure(E1000_interface *e) {
    e->txListPhys = dmaFrame();
    e->txList = (volatile E1000TX *)P2V(e->txListPhys);
    uint64_t bufPa = 0;
    for (int i = 0; i < E1000_TX_LIST_ENTRIES; i++) {
        if (!(i & 1))
            bufPa = dmaFrame();
        e->txBufPhys[i] = bufPa + (i & 1) * 2048;
        e->txBuf[i] = (uint8_t *)P2V(e->txBufPhys[i]);
    }
    E1000CmdWrite(e, REG_TXDESCLO, (uint32_t)e->txListPhys);
    E1000CmdWrite(e, REG_TXDESCHI, (uint32_t)(e->txListPhys >> 32));
    E1000CmdWrite(e, REG_TXDESCLEN, E1000_TX_LIST_ENTRIES * sizeof(E1000TX));
    /* mọi descriptor thuộc phần mềm: TDH == TDT */
    E1000CmdWrite(e, REG_TXDESCHEAD, 0);
    E1000CmdWrite(e, REG_TXDESCTAIL, 0);
    serial_puts("[pci::e1000] TX ring: 256 descriptors at PA ");
    serial_puthex_short(e->txListPhys);
    serial_puts(", one 2048 B buffer per descriptor, TDH=TDT=0\n");
}

/* ------------------------------------------------------------- ngắt */
static void rxDrain(E1000_interface *e) {
    for (;;) {
        volatile E1000RX *d = &e->rxList[e->rxHead];
        if (!(d->status & E1000RX_STATUS_DONE))
            break;                                   /* card chưa ghi descriptor này */
        if ((d->status & E1000RX_STATUS_END_OF_PACKET) && !d->errors) {
            netQueueAdd(e->nic, e->rxBuf[e->rxHead], d->length);
            e1000Stats.rxFrames++;
            e1000Stats.lastRxTick = timerTicks;
        }
        d->status = 0;
        d->errors = 0;
        E1000CmdWrite(e, REG_RXDESCTAIL, e->rxHead);  /* trả descriptor cho card */
        e->rxHead = (e->rxHead + 1) % E1000_RX_LIST_ENTRIES;
    }
}

/* = cavOS E1000InterruptHandler: đọc ICR (đọc là xoá -> card nhả đường INTx), xử lý từng bit */
static void E1000InterruptHandler(AsmPassedInterrupt *regs) {
    (void)regs;
    E1000_interface *e = selectedNIC->infoLocation;
    uint32_t status = E1000CmdRead(e, REG_ICR);
    e1000Stats.irqs++;
    if (!status) {
        e1000Stats.irqsEmpty++;
        return;
    }
    if (status & ICR_RX_TIMER_INTERRUPT) e1000Stats.rxt0++;
    if (status & ICR_RX_OVERRUN) e1000Stats.rxo++;
    if (status & ICR_RX_DESC_MIN_THRESHOLD_HIT) e1000Stats.rxdmt++;
    if (status & (ICR_RX_TIMER_INTERRUPT | ICR_RX_OVERRUN | ICR_RX_DESC_MIN_THRESHOLD_HIT))
        rxDrain(e);
    if (status & ICR_TX_DESC_WRITTEN_BACK) e1000Stats.txdw++;
    if (status & ICR_TX_QUEUE_EMPTY) e1000Stats.txqe++;
    if (status & ICR_LINK_STATUS_CHANGE) {
        e1000Stats.lsc++;
        E1000CmdWrite(e, REG_CTRL, E1000CmdRead(e, REG_CTRL) | CTRL_SET_LINK_UP);
    }
}

/* ------------------------------------------- dò GSI (lab, thay cho _PRT) */
static volatile int      probing;
static volatile uint32_t probeHits;
static uint8_t           probeVector[8];

static void probeHandler(AsmPassedInterrupt *regs) {
    if (!probing)
        return;
    for (int i = 0; i < 8; i++)
        if (regs->interrupt == probeVector[i]) {
            probeHits |= 1u << i;
            ioApicSetMask(16 + i, 1);                /* mỗi chân kêu tối đa 1 lần */
        }
    (void)E1000CmdRead(&e1000Info, REG_ICR);         /* card nhả INTx */
}

static int probeGsi(E1000_interface *e) {
    E1000CmdWrite(e, REG_IMASK_CLEAR, 0xffffffff);
    (void)E1000CmdRead(e, REG_ICR);
    for (int i = 0; i < 8; i++) {
        probeVector[i] = ioApicPciRedirect(16 + i, 1);   /* ghi entry, còn che */
        register_irq_handler(probeVector[i], probeHandler);
    }
    probing = 1;
    for (int i = 0; i < 8; i++)
        ioApicSetMask(16 + i, 0);
    E1000CmdWrite(e, REG_IMASK, ICR_LINK_STATUS_CHANGE);
    E1000CmdWrite(e, REG_ICS, ICR_LINK_STATUS_CHANGE);   /* card tự gây ngắt LSC */
    uint64_t until = timerTicks + 20;
    while (timerTicks < until)
        __asm__ volatile("hlt");
    probing = 0;
    E1000CmdWrite(e, REG_IMASK_CLEAR, 0xffffffff);
    for (int i = 0; i < 8; i++)
        ioApicSetMask(16 + i, 1);

    serial_puts("[pci::e1000] probe: GSI 16..23 -> vectors ");
    serial_puthex_short(probeVector[0]);
    serial_puts("..");
    serial_puthex_short(probeVector[7]);
    serial_puts(", wrote ICS=LSC, pins that fired within 20 ms:");
    int found = -1;
    for (int i = 0; i < 8; i++)
        if (probeHits & (1u << i)) {
            serial_puts(" GSI ");
            serial_putdec(16 + i);
            if (found < 0)
                found = 16 + i;
        }
    serial_puts(found < 0 ? " none\n" : "\n");
    return found;
}

/* ---------------------------------------------------------------- gửi */
E1000lastTx e1000LastTx;

/* = cavOS sendE1000: descriptor tại TDT, TDT++, chờ DD (nhường CPU trong lúc chờ) */
void sendE1000(NIC *nic, const void *packet, uint32_t packetSize) {
    E1000_interface *e = nic->infoLocation;
    spinlockAcquire(&LOCK_E1000_TX);                 /* cavOS không khoá (lwIP gọi tuần tự) */

    uint32_t tail = E1000CmdRead(e, REG_TXDESCTAIL);
    volatile E1000TX *desc = &e->txList[tail];
    memcpy(e->txBuf[tail], packet, packetSize);
    desc->addr = e->txBufPhys[tail];
    desc->length = (uint16_t)packetSize;
    desc->command = CMD_EOP | CMD_IFCS | CMD_RS;
    desc->status = 0;

    e1000LastTx.index = tail;
    e1000LastTx.tdhBefore = E1000CmdRead(e, REG_TXDESCHEAD);
    uint32_t next = (tail + 1) % E1000_TX_LIST_ENTRIES;
    uint64_t t0 = timerTicks;
    E1000CmdWrite(e, REG_TXDESCTAIL, next);          /* từ đây card sở hữu descriptor */

    uint64_t yields = 0;
    while (!(desc->status & 1)) {                    /* DD */
        handControl();
        yields++;
    }
    e1000LastTx.addr = desc->addr;
    e1000LastTx.length = desc->length;
    e1000LastTx.command = desc->command;
    e1000LastTx.status = desc->status;
    e1000LastTx.tdtAfter = next;
    e1000LastTx.tdhAfter = E1000CmdRead(e, REG_TXDESCHEAD);
    e1000LastTx.waitTicks = timerTicks - t0;
    e1000LastTx.yields = yields;
    e1000Stats.txFrames++;
    spinlockRelease(&LOCK_E1000_TX);
}

const uint8_t *e1000_tx_buffer(uint32_t index) { return e1000Info.txBuf[index % E1000_TX_LIST_ENTRIES]; }

void e1000_dump_rings(const char *when) {
    E1000_interface *e = &e1000Info;
    serial_puts(when);
    serial_puts("RDH=");
    serial_putdec(E1000CmdRead(e, REG_RXDESCHEAD));
    serial_puts(" RDT=");
    serial_putdec(E1000CmdRead(e, REG_RXDESCTAIL));
    serial_puts(" rxHead(driver)=");
    serial_putdec(e->rxHead);
    serial_puts("  TDH=");
    serial_putdec(E1000CmdRead(e, REG_TXDESCHEAD));
    serial_puts(" TDT=");
    serial_putdec(E1000CmdRead(e, REG_TXDESCTAIL));
    serial_putc('\n');
}

/* --------------------------------------------------------------- init */
int initiateE1000(PCIdevice *device) {
    if (!isE1000(device))
        return 0;

    PCIgeneralDevice details;
    GetGeneralDevice(device, &details);

    /* Bus master (bit 2): cho card TỰ đọc/ghi RAM (DMA) — thiếu bit này card không lấy
     * được descriptor. Memory space (bit 1): cho CPU truy cập BAR MMIO. Bit 10 = 1 thì
     * card không được kéo INTx. */
    uint16_t cmdBefore = device->command;
    uint16_t cmd = cmdBefore | PCI_CMD_BUS_MASTER | PCI_CMD_MEMORY;
    cmd &= ~PCI_CMD_INTX_DISABLE;
    /* cavOS ghi COMBINE_WORD(status, command): ghi lại status cũ vào 0x06, mà các bit lỗi
     * của status là RW1C (ghi 1 = xoá). Lab ghi status = 0 (không xoá gì). */
    ConfigWriteDword(device->bus, device->slot, device->function, PCI_COMMAND, cmd);
    uint16_t cmdAfter = ConfigReadWord(device->bus, device->slot, device->function, PCI_COMMAND);

    PCI *pci = lookupPCIdevice(device);
    setupPCIdeviceDriver(pci, PCI_DRIVER_E1000, PCI_DRIVER_CATEGORY_NIC);

    NIC *nic = createNewNIC(pci);
    nic->type = E1000;
    nic->mintu = 60;
    nic->irq = details.interruptLine;

    serial_puts("[pci::e1000] Intel E1000 NIC detected! dev{");
    serial_puthex_short(device->device_id);
    serial_puts("}\n[pci::e1000] PCI command ");
    serial_puthex_short(cmdBefore);
    serial_puts(" -> ");
    serial_puthex_short(cmdAfter);
    serial_puts(" (bit 2 bus master=");
    serial_putdec((cmdAfter >> 2) & 1);
    serial_puts(", bit 1 memory space=");
    serial_putdec((cmdAfter >> 1) & 1);
    serial_puts(", bit 10 INTx disable=");
    serial_putdec((cmdAfter >> 10) & 1);
    serial_puts(")\n");

    E1000_interface *e = &e1000Info;
    memset(e, 0, sizeof(*e));
    nic->infoLocation = e;
    e->nic = nic;
    e->deviceId = device->device_id;

    if (details.bar[0] & 1) {                    /* cavOS hỗ trợ cả I/O BAR; QEMU dùng MMIO */
        serial_puts("[pci::e1000] BAR0 is I/O: not handled in the lab\n");
        return 0;
    }
    PCIbar bar;
    pciBarProbe(device, 0, &bar);
    e->membasePhys = details.bar[0] & ~15u;
    e->mmioSize = bar.size;
    e->membase = mmio_map(e->membasePhys, bar.size);
    serial_puts("[pci::e1000] Fetched BAR[0] and MMIO is used: membase{");
    serial_puthex_short(e->membasePhys);
    serial_puts("} size ");
    serial_putdec(bar.size >> 10);
    serial_puts(" KiB -> mapped uncached (PCD|PWT) at VA ");
    serial_puthex_short(e->membase);
    serial_putc('\n');

    /* Reset (lab thêm): CTRL.RST tự xoá khi xong (8254x SDM §13.4.1, cần chờ >= 1 us) */
    uint32_t ctrl0 = E1000CmdRead(e, REG_CTRL);
    E1000CmdWrite(e, REG_CTRL, ctrl0 | CTRL_DEVICE_RESET);
    int polls = 0;
    while ((E1000CmdRead(e, REG_CTRL) & CTRL_DEVICE_RESET) && polls < 100000)
        polls++;
    E1000CmdWrite(e, REG_IMASK_CLEAR, 0xffffffff);  /* sau reset: che hết ngắt */
    serial_puts("[pci::e1000] device reset: CTRL ");
    serial_puthex_short(ctrl0);
    serial_puts(" |= RST (bit 26), bit cleared after ");
    serial_putdec(polls);
    serial_puts(" polls, CTRL now ");
    serial_puthex_short(E1000CmdRead(e, REG_CTRL));
    serial_puts(" (cavOS does not reset the card)\n");

    E1000EepromDetect(e);
    uint32_t eecd = E1000CmdRead(e, REG_EECD);
    if (!e->eeprom) {
        serial_puts("[pci::e1000] Todo: MAC parsing without EEPROM!\n");   /* cavOS: panic() */
        return 0;
    }
    uint16_t w0 = E1000EepromRead(e, 0), w1 = E1000EepromRead(e, 1), w2 = E1000EepromRead(e, 2);
    nic->MAC[0] = w0 & 0xff; nic->MAC[1] = w0 >> 8;
    nic->MAC[2] = w1 & 0xff; nic->MAC[3] = w1 >> 8;
    nic->MAC[4] = w2 & 0xff; nic->MAC[5] = w2 >> 8;
    serial_puts("[pci::e1000] EECD=");
    serial_puthex_short(eecd);
    serial_puts(" (bit 8 EEPROM present) -> EERD words 0,1,2 = ");
    serial_puthex_short(w0);
    serial_puts(" ");
    serial_puthex_short(w1);
    serial_puts(" ");
    serial_puthex_short(w2);
    serial_puts(" -> MAC ");
    macPrint(nic->MAC);
    serial_puts(" (little-endian words)\n");

    /* = cavOS: link reset 0, PHY reset 0, không VLAN, ILOS 0, SLU (set link up) 1 */
    uint32_t control = E1000CmdRead(e, REG_CTRL);
    control &= ~(CTRL_LINK_RESET | CTRL_PHY_RESET | CTRL_VLAN_MODE_ENABLE | CTRL_INVERT_LOSS_OF_SIGNAL);
    control |= CTRL_SET_LINK_UP;
    E1000CmdWrite(e, REG_CTRL, control);

    E1000RXConfigure(e);
    E1000TXConfigure(e);

    /* Ngắt. cavOS: targIrq = ioApicPciRegister(device, details) — tra _PRT (AML) của bus
     * PNP0A03 bằng uACPI -> GSI, rồi ioApicWriteRedEntry (level, cực tính theo _CRS). */
    serial_puts("[pci::e1000] config 0x3C Interrupt Line = ");
    serial_putdec(details.interruptLine);
    serial_puts(", Interrupt Pin = ");
    serial_putc(details.interruptPIN ? 'A' + details.interruptPIN - 1 : '-');
    serial_puts(" (Line = 8259 PIC IRQ chosen by firmware; useless once the I/O APIC is on)\n");
    int gsi = probeGsi(e);
    if (gsi < 0) {
        serial_puts("[pci::e1000] no GSI found -> giving up on interrupts\n");
        return 1;
    }
    e->gsi = (uint8_t)gsi;
    e->vector = ioApicPciRedirect(gsi, 0);           /* GSI đã có vector -> dùng lại */
    register_irq_handler(e->vector, E1000InterruptHandler);
    serial_puts("[pci::e1000] GSI ");
    serial_putdec(gsi);
    serial_puts(" -> vector ");
    serial_puthex_short(e->vector);
    serial_puts(", handler E1000InterruptHandler; I/O APIC entry:\n");
    ioapic_dump_entries(gsi, 1);

    /* = cavOS: IMS các nguyên nhân cần, rồi đọc ICR bỏ ngắt treo */
    E1000CmdWrite(e, REG_IMASK_CLEAR, 0xffffffff);
    E1000CmdWrite(e, REG_IMASK,
                  ICR_RX_TIMER_INTERRUPT | ICR_RX_OVERRUN | ICR_RX_DESC_MIN_THRESHOLD_HIT |
                      ICR_RX_SEQUENCE_ERROR | ICR_LINK_STATUS_CHANGE | ICR_TX_QUEUE_EMPTY |
                      ICR_TX_DESC_WRITTEN_BACK | ICR_TX_DESC_MIN_THRESHOLD_HIT);
    (void)E1000CmdRead(e, REG_ICR);

    E1000CmdWrite(e, REG_RX_CONTROL, E1000CmdRead(e, REG_RX_CONTROL) | RCTL_ENABLE);
    e1000RctlTick = timerTicks;
    E1000CmdWrite(e, REG_TCTL, E1000CmdRead(e, REG_TCTL) | TCTL_EN | TCTL_PSP);

    uint32_t st = E1000CmdRead(e, REG_STATUS);
    static const char *speed[4] = {"10", "100", "1000", "1000"};
    serial_puts("[pci::e1000] RCTL.EN + TCTL.EN set at t=");
    serial_putdec(e1000RctlTick);
    serial_puts("; STATUS=");
    serial_puthex_short(st);
    serial_puts(" -> link ");
    serial_puts(st & 2 ? "up" : "down");
    serial_puts(", ");
    serial_puts(speed[(st >> 6) & 3]);
    serial_puts(" Mb/s, ");
    serial_puts(st & 1 ? "full duplex\n" : "half duplex\n");
    return 1;
}
