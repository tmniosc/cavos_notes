/* pci.c — liệt kê PCI theo cavOS drivers/pci.c (Bài 14).
 *
 * Địa chỉ cấu hình (cơ chế #1, PCI Local Bus Spec 3.0 §3.2.2.3.2):
 *   bit 31 = enable, 23:16 = bus, 15:11 = device (slot), 10:8 = function, 7:2 = thanh ghi.
 * Ghi số đó vào cổng 0xCF8, rồi đọc/ghi 32 bit ở 0xCFC.
 *
 * Khác cavOS (có ghi trên trang lab):
 *  1. dsPCI là mảng tĩnh 32 ô (cavOS: LinkedList).
 *  2. initiatePCI() in một bảng (cavOS chỉ in khi driver nhận thiết bị) và đo cỡ BAR.
 *  3. Chỉ gọi driver NIC; AHCI (Bài 15) và VMware SVGA chỉ in ra "cavOS sẽ gọi ...".
 */
#include "pci.h"
#include "io.h"
#include "nic.h"
#include "serial.h"
#include "timer.h"

PCI dsPCI[32];
int dsPCIcount;

#define EXPORT_BYTE(target, first) ((first) ? ((target) & 0x00FF) : (((target) & 0xFF00) >> 8))
#define COMBINE_WORD(msb, lsb)     (((uint32_t)(msb) << 16) | (lsb))

static uint64_t configReads;       /* lab: đếm số lần đọc config để in */

/* ------------------------------------------------- đọc/ghi config space */
static uint32_t pciAddress(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) |
                      (offset & 0xFC) | 0x80000000u);
}

/* = cavOS ConfigReadWord: đọc 32 bit rồi lấy nửa theo bit 1 của offset */
uint16_t ConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    configReads++;
    outl(PCI_CONFIG_ADDRESS, pciAddress(bus, slot, func, offset));
    return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

uint32_t ConfigReadDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    configReads++;
    outl(PCI_CONFIG_ADDRESS, pciAddress(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

void ConfigWriteDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t conf) {
    outl(PCI_CONFIG_ADDRESS, pciAddress(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, conf);
}

/* = cavOS FilterDevice: vendor 0xFFFF (không ai trả lời) hoặc 0 thì bỏ */
int FilterDevice(uint8_t bus, uint8_t slot, uint8_t function) {
    uint16_t vendor_id = ConfigReadWord(bus, slot, function, PCI_VENDOR_ID);
    return !(vendor_id == 0xffff || !vendor_id);
}

/* = cavOS GetDevice: 16 byte đầu header (chung cho mọi header type) */
void GetDevice(PCIdevice *d, uint8_t bus, uint8_t slot, uint8_t function) {
    d->bus = bus;
    d->slot = slot;
    d->function = function;
    d->vendor_id = ConfigReadWord(bus, slot, function, PCI_VENDOR_ID);
    d->device_id = ConfigReadWord(bus, slot, function, PCI_DEVICE_ID);
    d->command = ConfigReadWord(bus, slot, function, PCI_COMMAND);
    d->status = ConfigReadWord(bus, slot, function, PCI_STATUS);
    uint16_t w = ConfigReadWord(bus, slot, function, PCI_REVISION_ID);
    d->revision = EXPORT_BYTE(w, 1);
    d->progIF = EXPORT_BYTE(w, 0);
    w = ConfigReadWord(bus, slot, function, PCI_SUBCLASS);
    d->subclass_id = EXPORT_BYTE(w, 1);
    d->class_id = EXPORT_BYTE(w, 0);
    w = ConfigReadWord(bus, slot, function, PCI_CACHE_LINE_SIZE);
    d->cacheLineSize = EXPORT_BYTE(w, 1);
    d->latencyTimer = EXPORT_BYTE(w, 0);
    w = ConfigReadWord(bus, slot, function, PCI_HEADER_TYPE);
    d->headerType = EXPORT_BYTE(w, 1);
    d->bist = EXPORT_BYTE(w, 0);
}

/* = cavOS GetGeneralDevice: phần riêng của header type 0 (0x10..0x3F) */
void GetGeneralDevice(PCIdevice *d, PCIgeneralDevice *out) {
    for (int i = 0; i < 6; i++)
        out->bar[i] = COMBINE_WORD(ConfigReadWord(d->bus, d->slot, d->function, PCI_BAR0 + 4 * i + 2),
                                   ConfigReadWord(d->bus, d->slot, d->function, PCI_BAR0 + 4 * i));
    out->system_vendor_id = ConfigReadWord(d->bus, d->slot, d->function, PCI_SYSTEM_VENDOR_ID);
    out->system_id = ConfigReadWord(d->bus, d->slot, d->function, PCI_SYSTEM_ID);
    out->expROMaddr = COMBINE_WORD(ConfigReadWord(d->bus, d->slot, d->function, PCI_EXP_ROM_BASE_ADDR + 2),
                                   ConfigReadWord(d->bus, d->slot, d->function, PCI_EXP_ROM_BASE_ADDR));
    out->capabilitiesPtr = EXPORT_BYTE(ConfigReadWord(d->bus, d->slot, d->function, PCI_CAPABILITIES_PTR), 1);
    uint16_t w = ConfigReadWord(d->bus, d->slot, d->function, PCI_INTERRUPT_LINE);
    out->interruptLine = EXPORT_BYTE(w, 1);
    out->interruptPIN = EXPORT_BYTE(w, 0);
    w = ConfigReadWord(d->bus, d->slot, d->function, PCI_MIN_GRANT);
    out->minGrant = EXPORT_BYTE(w, 1);
    out->maxLatency = EXPORT_BYTE(w, 0);
}

/* Lab: đo cỡ BAR theo PCI spec §6.2.5.1 — tắt decode, ghi toàn 1, đọc lại, trả giá trị cũ.
 * Bit thấp nhất còn đọc ra 1 (sau khi bỏ bit cờ) chính là cỡ vùng. */
int pciBarProbe(PCIdevice *d, int i, PCIbar *b) {
    uint8_t off = PCI_BAR0 + 4 * i;
    uint32_t cmd = ConfigReadDword(d->bus, d->slot, d->function, PCI_COMMAND);
    uint32_t raw = ConfigReadDword(d->bus, d->slot, d->function, off);

    *b = (PCIbar){0};
    b->raw = raw;
    /* tắt decode I/O + memory trong lúc BAR mang giá trị rác (giữ status = 0: RW1C) */
    ConfigWriteDword(d->bus, d->slot, d->function, PCI_COMMAND,
                     cmd & 0xFFFF & ~(PCI_CMD_IO | PCI_CMD_MEMORY));
    ConfigWriteDword(d->bus, d->slot, d->function, off, 0xFFFFFFFF);
    uint32_t probe = ConfigReadDword(d->bus, d->slot, d->function, off);
    ConfigWriteDword(d->bus, d->slot, d->function, off, raw);
    b->probe = probe;

    int used = 1;
    if (probe == 0) {                                /* BAR không cài đặt: đọc lại toàn 0 */
        ConfigWriteDword(d->bus, d->slot, d->function, PCI_COMMAND, cmd & 0xFFFF);
        return 1;
    }
    if (raw & 1) {                                   /* I/O BAR: bit 1:0 là cờ */
        b->io = 1;
        uint32_t mask = probe & ~0x3u & 0xFFFF;
        b->base = raw & ~0x3u;
        b->size = mask ? ((~mask + 1) & 0xFFFF) : 0;
    } else {                                         /* memory BAR: bit 3:0 là cờ */
        b->is64 = ((raw >> 1) & 3) == 2;
        b->prefetch = (raw >> 3) & 1;
        uint64_t mask = probe & ~0xFu;
        uint64_t base = raw & ~0xFu;
        if (b->is64 && i < 5) {
            uint8_t off2 = off + 4;
            uint32_t rawHi = ConfigReadDword(d->bus, d->slot, d->function, off2);
            ConfigWriteDword(d->bus, d->slot, d->function, off2, 0xFFFFFFFF);
            uint32_t probeHi = ConfigReadDword(d->bus, d->slot, d->function, off2);
            ConfigWriteDword(d->bus, d->slot, d->function, off2, rawHi);
            mask |= (uint64_t)probeHi << 32;
            base |= (uint64_t)rawHi << 32;
            used = 2;
        } else {
            mask |= 0xFFFFFFFF00000000ULL;
        }
        b->base = base;
        b->size = (mask & ~0xFULL) ? (~(mask & ~0xFULL) + 1) : 0;
    }
    b->present = b->size != 0;
    ConfigWriteDword(d->bus, d->slot, d->function, PCI_COMMAND, cmd & 0xFFFF);
    return used;
}

/* ------------------------------------------------------- dsPCI helpers */
PCI *lookupPCIdevice(PCIdevice *d) {
    for (int i = 0; i < dsPCIcount; i++)
        if (dsPCI[i].bus == d->bus && dsPCI[i].slot == d->slot && dsPCI[i].function == d->function)
            return &dsPCI[i];
    return 0;
}

void setupPCIdeviceDriver(PCI *pci, PCI_DRIVER driver, PCI_DRIVER_CATEGORY category) {
    pci->driver = driver;
    pci->category = category;
}

/* ------------------------------------------------------------- in bảng */
static const char *className(uint8_t c, uint8_t s) {
    switch (c) {
    case 0x01: return s == 0x06 ? "SATA controller (AHCI)" : s == 0x01 ? "IDE controller" : "Mass storage";
    case 0x02: return s == 0x00 ? "Ethernet controller" : "Network controller";
    case 0x03: return s == 0x00 ? "VGA-compatible display" : "Display controller";
    case 0x06:
        switch (s) {
        case 0x00: return "Host bridge";
        case 0x01: return "ISA bridge (LPC)";
        case 0x04: return "PCI-to-PCI bridge";
        default:   return "Bridge";
        }
    case 0x0C: return s == 0x05 ? "SMBus controller" : s == 0x03 ? "USB controller" : "Serial bus";
    default:   return "?";
    }
}

static void hex2(uint8_t v) {
    const char *h = "0123456789abcdef";
    serial_putc(h[v >> 4]);
    serial_putc(h[v & 15]);
}
static void hex4(uint16_t v) {
    hex2(v >> 8);
    hex2(v & 0xff);
}
static void bdf(PCIdevice *d) {
    hex2(d->bus);
    serial_putc(':');
    hex2(d->slot);
    serial_putc('.');
    serial_putdec(d->function);
}

static void printSize(uint64_t s) {
    if (s >= (1ULL << 20) && !(s & ((1ULL << 20) - 1))) {
        serial_putdec(s >> 20);
        serial_puts(" MiB");
    } else if (s >= 1024 && !(s & 1023)) {
        serial_putdec(s >> 10);
        serial_puts(" KiB");
    } else {
        serial_putdec(s);
        serial_puts(" B");
    }
}

static void printDevice(PCIdevice *d, PCIgeneralDevice *g) {
    serial_puts("[pci] ");
    bdf(d);
    serial_puts("  ");
    hex4(d->vendor_id);
    serial_putc(':');
    hex4(d->device_id);
    serial_puts("  class ");
    hex2(d->class_id);
    serial_putc('/');
    hex2(d->subclass_id);
    serial_putc('/');
    hex2(d->progIF);
    serial_puts("  hdr ");
    hex2(d->headerType);
    serial_puts("  cmd ");
    hex4(d->command);
    serial_puts("  IRQ line ");
    serial_putdec(g->interruptLine);
    serial_puts(" pin ");
    if (g->interruptPIN)
        serial_putc('A' + g->interruptPIN - 1);
    else
        serial_putc('-');
    serial_puts("  ");
    serial_puts(className(d->class_id, d->subclass_id));
    serial_putc('\n');

    for (int i = 0; i < 6;) {
        PCIbar b;
        int used = pciBarProbe(d, i, &b);
        if (b.present) {
            serial_puts("[pci]          BAR");
            serial_putdec(i);
            serial_puts(b.io ? " I/O     " : b.is64 ? " MMIO64  " : " MMIO32  ");
            serial_puthex_short(b.base);
            serial_puts("  size ");
            printSize(b.size);
            serial_puts("   (read ");
            serial_puthex_short(b.raw);
            serial_puts(", after writing 0xffffffff read ");
            serial_puthex_short(b.probe);
            serial_puts(b.prefetch ? ", prefetchable)\n" : ")\n");
        }
        i += used;
    }
}

/* = cavOS initiatePCI(): quét vét cạn mọi bus/slot/function */
void initiatePCI(void) {
    PCIdevice device;
    int present = 0, general = 0;
    configReads = 0;
    uint64_t t0 = timerTicks;

    serial_puts("[pci] scanning like cavOS initiatePCI(): bus 0..255 x slot 0..31 x function 0..7"
                " through 0xCF8/0xCFC\n");
    serial_puts("[pci] B:S.F      ven:dev    class/sub/if hdr       cmd       IRQ\n");

    for (uint16_t bus = 0; bus < PCI_MAX_BUSES; bus++) {
        for (uint8_t slot = 0; slot < PCI_MAX_DEVICES; slot++) {
            for (uint8_t function = 0; function < PCI_MAX_FUNCTIONS; function++) {
                if (!FilterDevice(bus, slot, function))
                    continue;
                present++;

                GetDevice(&device, bus, slot, function);
                if ((device.headerType & ~(1 << 7)) != PCI_DEVICE_GENERAL) {
                    serial_puts("[pci] ");
                    bdf(&device);
                    serial_puts("  header type != 0 -> skipped (cavOS: continue)\n");
                    continue;
                }
                general++;

                PCIgeneralDevice g;
                GetGeneralDevice(&device, &g);
                printDevice(&device, &g);

                if (dsPCIcount >= 32)
                    continue;
                PCI *target = &dsPCI[dsPCIcount++];
                *target = (PCI){0};
                target->bus = bus;
                target->slot = slot;
                target->function = function;
                target->vendor_id = device.vendor_id;
                target->device_id = device.device_id;

                switch (device.class_id) {
                case PCI_CLASS_CODE_NETWORK_CONTROLLER:
                    serial_puts("[pci]          -> class 0x02: initiateNIC()\n");
                    initiateNIC(&device);
                    break;
                case PCI_CLASS_CODE_MASS_STORAGE_CONTROLLER:
                    if (device.subclass_id == 0x6)
                        serial_puts("[pci]          -> class 0x01 sub 0x06: cavOS calls initiateAHCI()"
                                    " (Bai 15); lab skips it\n");
                    break;
                case PCI_CLASS_CODE_DISPLAY_CONTROLLER:
                    serial_puts("[pci]          -> class 0x03: cavOS calls initiateVMWareSvga2(),"
                                " which only takes 15ad:0405; lab skips it\n");
                    break;
                default:
                    break;
                }
            }
        }
    }

    serial_puts("[pci] done: 65536 (bus,slot,function) addresses probed, ");
    serial_putdec(present);
    serial_puts(" functions answered, ");
    serial_putdec(general);
    serial_puts(" with header type 0; ");
    serial_putdec(configReads);
    serial_puts(" config reads, ");
    serial_putdec(timerTicks - t0);
    serial_puts(" ms (scan + initiateE1000)\n");
}
