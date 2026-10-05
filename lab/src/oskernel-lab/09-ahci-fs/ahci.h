/* ahci.h — controller SATA AHCI theo cavOS include/ahci.h + drivers/ahci.c (Bài 15).
 * Bố cục thanh ghi theo AHCI 1.3.1 (§3: HBA memory registers, §4.2: command list/table).
 */
#pragma once
#include <stdint.h>
#include "pci.h"

#define SECTOR_SIZE 512

/* ---- thanh ghi của cả controller (BAR5 = "ABAR") -------------------- */
typedef volatile struct {
    uint32_t clb, clbu;     /* 0x00 command list base (PA, 1 KiB aligned) */
    uint32_t fb, fbu;       /* 0x08 received-FIS base (PA, 256 B aligned) */
    uint32_t is;            /* 0x10 interrupt status */
    uint32_t ie;            /* 0x14 interrupt enable */
    uint32_t cmd;           /* 0x18 command and status: ST, FRE, FR, CR */
    uint32_t rsv0;
    uint32_t tfd;           /* 0x20 task file data: bit 7 BSY, bit 3 DRQ, bit 0 ERR */
    uint32_t sig;           /* 0x24 signature: 0x00000101 = SATA disk */
    uint32_t ssts;          /* 0x28 SATA status: DET (3:0), IPM (11:8) */
    uint32_t sctl, serr, sact;
    uint32_t ci;            /* 0x38 command issue: 1 bit per slot */
    uint32_t sntf, fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} HBA_PORT;                 /* 0x80 bytes */

typedef volatile struct {
    uint32_t cap;           /* 0x00 capabilities: NP (4:0) = ports - 1, NCS (12:8) = slots - 1 */
    uint32_t ghc;           /* 0x04 global host control: HR (0), IE (1), AE (31) */
    uint32_t is;            /* 0x08 interrupt status, 1 bit per port */
    uint32_t pi;            /* 0x0C ports implemented */
    uint32_t vs;            /* 0x10 version */
    uint32_t ccc_ctl, ccc_pts, em_loc, em_ctl;
    uint32_t cap2;          /* 0x24 */
    uint32_t bohc;          /* 0x28 BIOS/OS handoff: BOS (0), OOS (1), BB (4) */
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    HBA_PORT ports[32];     /* 0x100 */
} HBA_MEM;

/* ---- command list: 32 header x 32 B ------------------------------------ */
typedef struct {
    uint8_t  cfl : 5;       /* command FIS length, in dwords */
    uint8_t  a : 1, w : 1, p : 1;
    uint8_t  r : 1, b : 1, c : 1, rsv0 : 1, pmp : 4;
    uint16_t prdtl;         /* PRDT entries */
    volatile uint32_t prdbc;/* bytes transferred (written by the HBA) */
    uint32_t ctba, ctbau;   /* command table PA (128 B aligned) */
    uint32_t rsv1[4];
} __attribute__((packed)) HBA_CMD_HEADER;

typedef struct {
    uint32_t dba, dbau;     /* data buffer PA */
    uint32_t rsv0;
    uint32_t dbc : 22;      /* byte count - 1 (max 4 MiB) */
    uint32_t rsv1 : 9;
    uint32_t i : 1;         /* interrupt on completion */
} __attribute__((packed)) HBA_PRDT_ENTRY;

#define AHCI_PRDTS 8
typedef struct {
    uint8_t cfis[64];       /* command FIS */
    uint8_t acmd[16];
    uint8_t rsv[48];
    HBA_PRDT_ENTRY prdt_entry[AHCI_PRDTS];
} __attribute__((packed)) HBA_CMD_TBL;   /* 128 + 16 * 8 = 256 B */
#define AHCI_MEM_TABLE 256

/* ---- FIS "register, host to device": 20 byte lệnh ATA ------------------- */
#define FIS_TYPE_REG_H2D 0x27
typedef struct {
    uint8_t fis_type;
    uint8_t pmport : 4, rsv0 : 3, c : 1;   /* c = 1: this is a command */
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0, lba1, lba2, device;
    uint8_t lba3, lba4, lba5, featureh;
    uint8_t countl, counth, icc, control;
    uint8_t rsv1[4];
} __attribute__((packed)) FIS_REG_H2D;

#define ATA_CMD_READ_DMA_EX  0x25
#define ATA_CMD_IDENTIFY     0xEC
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ  0x08

#define HBA_PxCMD_ST  (1u << 0)
#define HBA_PxCMD_FRE (1u << 4)
#define HBA_PxCMD_FR  (1u << 14)
#define HBA_PxCMD_CR  (1u << 15)
#define SATA_SIG_ATA  0x00000101

typedef struct {
    HBA_MEM *mem;
    uint32_t sata;          /* bitmask: ports with a SATA disk (cavOS: ahci.sata) */
    HBA_CMD_HEADER *clbVirt[32];
    uint8_t *ctbaVirt[32][2];  /* 2 frames of command tables: slots 0..15, 16..31 */
    uint64_t ctbaPhys[32][2];
} ahci;

extern ahci ahciInfo;
int  initiateAHCI(PCIdevice *device);
/* đọc `count` sector từ LBA vào buff (VA trong kernel, mỗi trang tra PA riêng) */
int  ahciRead(ahci *a, uint32_t portId, uint64_t lba, uint32_t count, uint8_t *buff);
extern uint64_t ahciSectorsRead, ahciCommands;
