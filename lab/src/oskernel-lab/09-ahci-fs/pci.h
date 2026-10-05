/* pci.h — PCI config space qua cơ chế #1 (cổng 0xCF8/0xCFC), theo cavOS include/pci.h +
 * drivers/pci.c (Bài 14).
 *
 * Giữ y cavOS: tên hàm (ConfigReadWord, ConfigWriteDword, FilterDevice, GetDevice,
 * GetGeneralDevice, initiatePCI), struct PCIdevice / PCIgeneralDevice, vòng quét
 * bus 0..255 x slot 0..31 x function 0..7, chỉ nhận header type 0 rồi phân loại theo class.
 * Rút gọn: danh sách dsPCI là mảng tĩnh (cavOS: LinkedList + malloc), bỏ GetParentBridge.
 * Thêm của lab: đo kích thước BAR (cavOS chỉ đọc giá trị BAR, không đo), bảng in ra.
 */
#pragma once
#include <stdint.h>

/* Giới hạn */
#define PCI_MAX_BUSES     256
#define PCI_MAX_DEVICES   32
#define PCI_MAX_FUNCTIONS 8

/* Cơ chế cấu hình #1: ghi địa chỉ vào 0xCF8, đọc/ghi dữ liệu ở 0xCFC */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef enum {
    PCI_DEVICE_GENERAL = 0x0,
    PCI_DEVICE_BRIDGE  = 0x1,
    PCI_DEVICE_CARDBUS = 0x2,
} PCI_DEVICES;

/* Offset trong header type 0 (PCI Local Bus Spec 3.0 §6.1) — giá trị y cavOS */
#define PCI_VENDOR_ID         0x00
#define PCI_DEVICE_ID         0x02
#define PCI_COMMAND           0x04
#define PCI_STATUS            0x06
#define PCI_REVISION_ID       0x08
#define PCI_SUBCLASS          0x0a
#define PCI_CACHE_LINE_SIZE   0x0c
#define PCI_HEADER_TYPE       0x0e
#define PCI_BAR0              0x10
#define PCI_SYSTEM_VENDOR_ID  0x2C
#define PCI_SYSTEM_ID         0x2E
#define PCI_EXP_ROM_BASE_ADDR 0x30
#define PCI_CAPABILITIES_PTR  0x34
#define PCI_INTERRUPT_LINE    0x3C
#define PCI_MIN_GRANT         0x3E

/* Bit trong thanh ghi Command */
#define PCI_CMD_IO          (1 << 0)
#define PCI_CMD_MEMORY      (1 << 1)
#define PCI_CMD_BUS_MASTER  (1 << 2)
#define PCI_CMD_INTX_DISABLE (1 << 10)

#define PCI_CLASS_CODE_MASS_STORAGE_CONTROLLER 0x1
#define PCI_CLASS_CODE_NETWORK_CONTROLLER      0x2
#define PCI_CLASS_CODE_DISPLAY_CONTROLLER      0x3

typedef struct {
    uint16_t bus, slot, function;
    uint16_t vendor_id, device_id;
    uint16_t command, status;
    uint8_t  revision, progIF, subclass_id, class_id;
    uint8_t  cacheLineSize, latencyTimer, headerType, bist;
} PCIdevice;

typedef struct {
    uint32_t bar[6];
    uint16_t system_id, system_vendor_id;
    uint32_t expROMaddr;
    uint8_t  capabilitiesPtr;
    uint8_t  interruptLine, interruptPIN, minGrant, maxLatency;
} PCIgeneralDevice;

typedef enum { PCI_DRIVER_NULL = 0, PCI_DRIVER_AHCI, PCI_DRIVER_RTL8139,
               PCI_DRIVER_RTL8169, PCI_DRIVER_E1000 } PCI_DRIVER;
typedef enum { PCI_DRIVER_CATEGORY_NULL = 0, PCI_DRIVER_CATEGORY_STORAGE,
               PCI_DRIVER_CATEGORY_NIC } PCI_DRIVER_CATEGORY;

/* = struct PCI của cavOS (một node trong dsPCI) */
typedef struct {
    uint8_t  bus, slot, function;
    uint16_t vendor_id, device_id;
    PCI_DRIVER          driver;
    PCI_DRIVER_CATEGORY category;
    void    *extra;                 /* NIC* khi category = NIC */
} PCI;

extern PCI dsPCI[32];
extern int dsPCIcount;

/* Kết quả đo một BAR (lab) */
typedef struct {
    int      present;
    int      io;                    /* 1 = I/O port, 0 = MMIO */
    int      is64;                  /* MMIO 64 bit: chiếm cả BAR kế tiếp */
    int      prefetch;
    uint64_t base;
    uint64_t size;
    uint32_t raw, probe;            /* giá trị gốc và giá trị đọc lại sau khi ghi 0xFFFFFFFF */
} PCIbar;

uint16_t ConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t ConfigReadDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     ConfigWriteDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t conf);

int  FilterDevice(uint8_t bus, uint8_t slot, uint8_t function);
void GetDevice(PCIdevice *device, uint8_t bus, uint8_t slot, uint8_t function);
void GetGeneralDevice(PCIdevice *device, PCIgeneralDevice *out);
int  pciBarProbe(PCIdevice *device, int index, PCIbar *out);   /* trả số BAR đã dùng (1/2) */

PCI *lookupPCIdevice(PCIdevice *device);
void setupPCIdeviceDriver(PCI *pci, PCI_DRIVER driver, PCI_DRIVER_CATEGORY category);

void initiatePCI(void);
