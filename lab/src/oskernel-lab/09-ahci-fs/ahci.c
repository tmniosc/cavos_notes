/* ahci.c — driver SATA AHCI theo cavOS drivers/ahci.c (Bài 15).
 *
 * Giống cavOS (cùng thứ tự trong initiateAHCI): nhận controller theo vendor:device -> bật bus
 * master + memory space -> BAR5 (ABAR) -> reset HBA (GHC.HR) -> BIOS/OS handoff (BOHC)
 * -> GHC.AE -> dò từng cổng trong PI (SSTS.DET/IPM, SIG) -> "rebase" cổng có đĩa SATA:
 * dừng engine, cấp command list + vùng FIS nhận + command table, chạy lại engine.
 * Đọc = tìm slot trống (SACT|CI), điền command header + command table (FIS READ DMA EXT,
 * PRDT), ghi CI = 1 << slot, chờ bit đó về 0.
 *
 * Khác cavOS (có ghi trên trang lab):
 *  1. ABAR map uncached vào VA riêng (cavOS: hhdmOffset + phys).
 *  2. PRDT: mỗi trang của buffer một entry, mỗi entry tra PA riêng (vresolve). cavOS tra PA
 *     của trang ĐẦU rồi cho một entry dài tới 4 MiB, đúng chỉ khi buffer liền về vật lý.
 *     DEMO=-DPRDT_CAVOS làm y cavOS để thấy chỗ sai.
 *  3. Không đăng ký IRQ: cavOS cũng chờ bằng cách hỏi CI, handler của nó chỉ xoá IS.
 *  4. Thêm lệnh IDENTIFY để in tên ổ và số sector (cavOS không gửi).
 *  5. Phân loại cổng lại SAU khi bật FRE: ngay sau reset HBA, SIG còn là 0xFFFFFFFF, nên
 *     cách của cavOS (đọc SIG trước) coi mọi thiết bị là đĩa SATA, kể cả ổ CD.
 */
#include "ahci.h"
#include "boot.h"
#include "io.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "timer.h"

ahci ahciInfo;
uint64_t ahciSectorsRead, ahciCommands;

static void hex(uint64_t v) { serial_puthex_short(v); }

/* ------------------------------------------------------ command engine */
static void ahciCmdStart(HBA_PORT *port) {
    while (port->cmd & HBA_PxCMD_CR)
        ;
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

static void ahciCmdStop(HBA_PORT *port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    while (port->cmd & (HBA_PxCMD_FR | HBA_PxCMD_CR))
        ;
}

static int ahciCmdFind(HBA_PORT *port) {
    uint32_t slots = port->sact | port->ci;     /* bit 0 ở cả hai = slot rảnh */
    for (int i = 0; i < 32; i++)
        if (!(slots & (1u << i)))
            return i;
    return -1;
}

static uint64_t frame0(void) {
    uint64_t pa = pmm_alloc();
    memset(P2V(pa), 0, PAGE_SIZE);
    return pa;
}

/* ------------------------------------------------------ cổng */
static const char *portType(HBA_PORT *port, int *isSata) {
    uint32_t ssts = port->ssts;
    uint8_t det = ssts & 0xF, ipm = (ssts >> 8) & 0xF;
    *isSata = 0;
    if (det != 3) return "no device (DET != 3)";
    if (ipm != 1) return "device not active (IPM != 1)";
    switch (port->sig) {
    case 0xEB140101: return "SATAPI (CD/DVD)";
    case 0xC33C0101: return "SEMB";
    case 0x96690101: return "port multiplier";
    default: *isSata = 1; return "SATA disk";
    }
}

static void ahciPortRebase(ahci *a, HBA_PORT *port, int portno) {
    ahciCmdStop(port);

    uint64_t clb = frame0();           /* 32 header x 32 B = 1 KiB (cavOS: VirtualAllocate) */
    port->clb = (uint32_t)clb;
    port->clbu = (uint32_t)(clb >> 32);
    a->clbVirt[portno] = (HBA_CMD_HEADER *)P2V(clb);

    uint64_t fb = frame0();            /* received FIS: 256 B */
    port->fb = (uint32_t)fb;
    port->fbu = (uint32_t)(fb >> 32);

    for (int h = 0; h < 2; h++) {      /* 32 command table x 256 B = 2 trang */
        a->ctbaPhys[portno][h] = frame0();
        a->ctbaVirt[portno][h] = P2V(a->ctbaPhys[portno][h]);
    }
    for (int i = 0; i < 32; i++) {
        uint64_t ct = a->ctbaPhys[portno][i / 16] + (i % 16) * AHCI_MEM_TABLE;
        a->clbVirt[portno][i].prdtl = AHCI_PRDTS;
        a->clbVirt[portno][i].ctba = (uint32_t)ct;
        a->clbVirt[portno][i].ctbau = (uint32_t)(ct >> 32);
    }
    port->serr = port->serr;           /* xoá lỗi cũ (ghi 1 để xoá) */
    port->is = (uint32_t)-1;
    ahciCmdStart(port);
    a->sata |= 1u << portno;

    serial_puts("[pci::ahci]   port ");
    serial_putdec(portno);
    serial_puts(" rebased: command list PA ");
    hex(clb);
    serial_puts(", received FIS PA ");
    hex(fb);
    serial_puts(", command tables PA ");
    hex(a->ctbaPhys[portno][0]);
    serial_puts(" + ");
    hex(a->ctbaPhys[portno][1]);
    serial_puts(", CMD ");
    hex(port->cmd);
    serial_puts(" (ST=1 FRE=1)\n");
}

/* ------------------------------------------------------ một lệnh ATA */
static int ahciIssue(ahci *a, uint32_t portId, uint8_t command, uint64_t lba, uint32_t count,
                     uint8_t *buff, uint32_t bytes) {
    HBA_PORT *port = &a->mem->ports[portId];
    port->is = (uint32_t)-1;
    int slot = ahciCmdFind(port);
    if (slot < 0)
        return 0;

    HBA_CMD_HEADER *h = &a->clbVirt[portId][slot];
    HBA_CMD_TBL *t = (HBA_CMD_TBL *)(a->ctbaVirt[portId][slot / 16] + (slot % 16) * AHCI_MEM_TABLE);
    memset(t, 0, sizeof(HBA_CMD_TBL));
    h->cfl = sizeof(FIS_REG_H2D) / 4;
    h->w = 0;
    h->prdbc = 0;

    int n = 0;
#ifdef PRDT_CAVOS
    /* y cavOS ahciSetUpCmd: PA của trang đầu, một entry cho cả buffer */
    uint64_t pa = vresolve((uint64_t)buff);
    t->prdt_entry[0].dba = (uint32_t)pa;
    t->prdt_entry[0].dbau = (uint32_t)(pa >> 32);
    t->prdt_entry[0].dbc = bytes - 1;
    t->prdt_entry[0].i = 1;
    n = 1;
#else
    /* mỗi đoạn không vượt qua ranh giới trang có một entry, với PA của chính trang đó */
    uint64_t va = (uint64_t)buff, left = bytes;
    while (left) {
        uint64_t chunk = PAGE_SIZE - (va & 0xFFF);
        if (chunk > left) chunk = left;
        uint64_t pa = vresolve(va);
        if (n == AHCI_PRDTS) return 0;
        t->prdt_entry[n].dba = (uint32_t)pa;
        t->prdt_entry[n].dbau = (uint32_t)(pa >> 32);
        t->prdt_entry[n].dbc = chunk - 1;
        t->prdt_entry[n].i = 1;
        n++;
        va += chunk;
        left -= chunk;
    }
#endif
    h->prdtl = n;

    FIS_REG_H2D *f = (FIS_REG_H2D *)t->cfis;
    f->fis_type = FIS_TYPE_REG_H2D;
    f->c = 1;
    f->command = command;
    f->lba0 = lba;
    f->lba1 = lba >> 8;
    f->lba2 = lba >> 16;
    f->device = 1 << 6;                /* LBA mode */
    f->lba3 = lba >> 24;
    f->lba4 = lba >> 32;
    f->lba5 = lba >> 40;
    f->countl = count & 0xFF;
    f->counth = (count >> 8) & 0xFF;

    uint64_t t0 = timerTicks;
    while (port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ))
        if (timerTicks > t0 + 1000) return 0;

    port->ci = 1u << slot;             /* giao lệnh cho HBA */
    while (port->ci & (1u << slot))    /* HBA xoá bit khi xong */
        if (timerTicks > t0 + 1000) return 0;
    if (port->is & (1u << 30)) {       /* TFES: task file error */
        serial_puts("[pci::ahci] task file error, TFD ");
        hex(port->tfd);
        serial_putc('\n');
        return 0;
    }
    ahciCommands++;
    return 1;
}

int ahciRead(ahci *a, uint32_t portId, uint64_t lba, uint32_t count, uint8_t *buff) {
    int ok = ahciIssue(a, portId, ATA_CMD_READ_DMA_EX, lba, count, buff, count * SECTOR_SIZE);
    if (ok) ahciSectorsRead += count;
    return ok;
}

/* IDENTIFY DEVICE: 512 byte mô tả ổ. Chuỗi ATA lưu từng cặp byte đảo chỗ. */
static void ahciIdentify(ahci *a, int portId) {
    static uint16_t id[256] __attribute__((aligned(4096)));
    if (!ahciIssue(a, portId, ATA_CMD_IDENTIFY, 0, 0, (uint8_t *)id, 512)) {
        serial_puts("[pci::ahci]   IDENTIFY failed\n");
        return;
    }
    char model[41];
    for (int i = 0; i < 20; i++) {
        model[2 * i] = id[27 + i] >> 8;
        model[2 * i + 1] = id[27 + i] & 0xFF;
    }
    model[40] = 0;
    for (int i = 39; i >= 0 && model[i] == ' '; i--) model[i] = 0;
    uint64_t sectors = (uint64_t)id[100] | ((uint64_t)id[101] << 16) | ((uint64_t)id[102] << 32);
    serial_puts("[pci::ahci]   IDENTIFY: model \"");
    serial_puts(model);
    serial_puts("\", LBA48 ");
    serial_puts((id[83] & (1 << 10)) ? "yes" : "no");
    serial_puts(", ");
    serial_putdec(sectors);
    serial_puts(" sectors x 512 B = ");
    serial_putdec(sectors / 2048);
    serial_puts(" MiB\n");
}

/* ------------------------------------------------------ khởi tạo */
int initiateAHCI(PCIdevice *device) {
    if (!(device->vendor_id == 0x8086 && device->device_id == 0x2922))   /* cavOS: bảng ahci_ids */
        return 0;
    serial_puts("[pci::ahci] Detected controller! name{Intel ICH9}\n");

    PCIgeneralDevice g;
    GetGeneralDevice(device, &g);
    uint32_t cmd = ConfigReadDword(device->bus, device->slot, device->function, PCI_COMMAND) & 0xFFFF;
    uint32_t newCmd = (cmd | (1 << 2) | (1 << 1)) & ~(1u << 10);
    ConfigWriteDword(device->bus, device->slot, device->function, PCI_COMMAND, newCmd);
    serial_puts("[pci::ahci] PCI command ");
    hex(cmd);
    serial_puts(" -> ");
    hex(newCmd);
    serial_puts(" (bus master, memory space)\n");

    uint64_t abar = g.bar[5] & 0xFFFFFFF0;
    HBA_MEM *mem = (HBA_MEM *)mmio_map(abar, 0x2000);
    ahci *a = &ahciInfo;
    a->mem = mem;
    PCI *pci = lookupPCIdevice(device);
    if (pci) {
        setupPCIdeviceDriver(pci, PCI_DRIVER_AHCI, PCI_DRIVER_CATEGORY_STORAGE);
        pci->extra = a;
    }

    uint32_t cap = mem->cap, vs = mem->vs;
    serial_puts("[pci::ahci] ABAR (BAR5) PA ");
    hex(abar);
    serial_puts(" -> VA ");
    hex((uint64_t)mem);
    serial_puts("; CAP ");
    hex(cap);
    serial_puts(" -> ");
    serial_putdec((cap & 0x1F) + 1);
    serial_puts(" ports, ");
    serial_putdec(((cap >> 8) & 0x1F) + 1);
    serial_puts(" command slots, 64-bit DMA ");
    serial_puts((cap >> 31) ? "yes" : "no");
    serial_puts("; version ");
    serial_putdec(vs >> 16);
    serial_putc('.');
    serial_putdec((vs >> 8) & 0xFF);
    serial_putdec(vs & 0xFF);
    serial_puts("; PI ");
    hex(mem->pi);
    serial_putc('\n');

    /* reset cả HBA (AHCI 1.3.1 §10.4.3): ghi GHC.HR, chờ HBA tự xoá */
    uint64_t t0 = timerTicks;
    mem->ghc |= 1;
    while (mem->ghc & 1)
        ;
    serial_puts("[pci::ahci] HBA reset (GHC.HR) done in ");
    serial_putdec(timerTicks - t0);
    serial_puts(" ms; BOHC ");
    hex(mem->bohc);
    serial_puts(", CAP2 ");
    hex(mem->cap2);
    if (mem->cap2 & 1) {               /* BOH: controller có cơ chế handoff */
        mem->bohc |= 2;                /* OOS: OS xin quyền */
        while (mem->bohc & 1)          /* chờ BIOS nhả BOS */
            ;
        serial_puts(" -> BIOS/OS handoff done");
    } else {
        serial_puts(" (CAP2.BOH = 0: no BIOS/OS handoff on this controller)");
    }
    mem->ghc |= 1u << 31;              /* AE: chạy ở chế độ AHCI */
    serial_puts("\n[pci::ahci] GHC ");
    hex(mem->ghc);
    serial_puts(" (AE=1)\n");

    uint32_t pi = mem->pi;
    for (int i = 0; i < 32; i++) {
        if (!(pi & (1u << i)))
            continue;
        HBA_PORT *port = &mem->ports[i];
        int isSata;
        uint32_t sigBefore = port->sig;
        const char *type = portType(port, &isSata);   /* = chỗ cavOS phân loại */
        serial_puts("[pci::ahci] port ");
        serial_putdec(i);
        serial_puts(": SSTS ");
        hex(port->ssts);
        serial_puts(" SIG ");
        hex(sigBefore);
        serial_puts(" -> ");
        serial_puts(type);
        serial_puts(isSata ? " (cavOS's classification)\n" : "\n");
        if (!isSata)
            continue;
        /* Sau reset HBA, SIG = 0xFFFFFFFF cho tới khi ổ gửi FIS đầu tiên, mà HBA chỉ nhận FIS
         * khi FRE = 1. Bật engine (rebase) rồi chờ SIG thật. */
        ahciPortRebase(a, port, i);
        uint64_t t1 = timerTicks;
        while (port->sig == 0xFFFFFFFF && timerTicks < t1 + 100)
            ;
        type = portType(port, &isSata);
        serial_puts("[pci::ahci]   after FRE=1: SIG ");
        hex(port->sig);
        serial_puts(" (after ");
        serial_putdec(timerTicks - t1);
        serial_puts(" ms) -> ");
        serial_puts(type);
        serial_putc('\n');
        if (isSata) {
            ahciIdentify(a, i);
        } else {
            ahciCmdStop(port);
            a->sata &= ~(1u << i);
            serial_puts("[pci::ahci]   not a SATA disk: engine stopped, port dropped\n");
        }
    }
    return 1;
}
