/* Lab 0x08 — PCI & NIC (Bài 13 + Bài 14).
 * Thứ tự giống cavOS _start():
 *   ... initiateKb -> initiateMouse -> initiateTasks -> initiateKernelThreads
 *   -> initiateNetworking()   (selectedNIC = 0, chưa chạm phần cứng)
 *   -> initiatePCI()          (quét bus; card mạng -> initiateNIC -> initiateE1000)
 * Sau đó thread "net" làm việc lwIP làm trong cavOS ngay khi có NIC (dhcp_start), nhưng
 * bằng frame tự dựng: DHCP DISCOVER/OFFER/REQUEST/ACK, ARP hỏi MAC của router, 2 ping.
 * Frame nhận đi đúng đường cavOS: IRQ -> netQueueAdd -> thread helper -> helperNet ->
 * handlePacket -> net_input (cavOS: lwIP).
 * DEMO: -DEOI_LATE (EOI sau handler), -DWAIT_RCTL (gửi DISCOVER muộn 1,1 s),
 *       -DPCI_ACTIVE_LOW (entry I/O APIC active-low).
 * Chạy: make run (QEMU user network + filter-dump ghi lab08.pcap), rồi
 *       python3 scripts/pcap_summary.py lab08.pcap
 */
#include "../limine.h"
#include "boot.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "acpi.h"
#include "apic.h"
#include "timer.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "serial.h"
#include "task.h"
#include "kernel_helper.h"
#include "kout.h"
#include "pci.h"
#include "nic.h"
#include "e1000.h"
#include "net.h"

LIMINE_BASE_REVISION(2)

static void halt(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

static volatile int netDone, netOk;

static void putIp(const uint8_t *ip) {
    for (int i = 0; i < 4; i++) {
        serial_putdec(ip[i]);
        if (i < 3) serial_putc('.');
    }
}

static void step(const char *s) {
    kout_begin();
    serial_puts(s);
    serial_putc('\n');
    kout_end();
}

/* Thread "net": cavOS chạy lwIP trong thread tcpip (tcpip_init -> lwipInitInThread ->
 * dhcp_start). Ở đây một kernel thread thường làm 3 việc tương ứng bằng tay. */
static void netThread(uint64_t rdi) {
    (void)rdi;
    NIC *nic = selectedNIC;

    kout_begin();
    e1000_dump_rings("rings before: ");
    kout_end();

#ifdef WAIT_RCTL
    /* DEMO: chờ tới 1100 ms sau lần ghi RCTL cuối rồi mới gửi DISCOVER */
    kout_begin();
    serial_puts("WAIT_RCTL: RCTL last written at t=");
    serial_putdec(e1000RctlTick);
    serial_puts(", sleeping until t=");
    serial_putdec(e1000RctlTick + 1100);
    serial_putc('\n');
    kout_end();
    while (timerTicks < e1000RctlTick + 1100)
        taskSleepMs(10);
#endif
    step("step 1: DHCP (what lwIP dhcp_start() does in cavOS): DISCOVER -> OFFER -> REQUEST -> ACK");
    if (!net_dhcp(nic)) {
        step("DHCP failed (no OFFER/ACK within 2 s)");
        netDone = 1;
        return;
    }
    kout_begin();
    serial_puts("configured: ip ");
    putIp(nic->ip);
    serial_puts(" mask ");
    putIp(netMask);
    serial_puts(" router ");
    putIp(netRouter);
    serial_puts(" dns ");
    putIp(netDns);
    serial_putc('\n');
    kout_end();

    step("step 2: ARP - who has the router's IP?");
    uint8_t routerMac[6];
    if (!net_arp_resolve(nic, netRouter, routerMac)) {
        step("ARP failed");
        netDone = 1;
        return;
    }

    step("step 3: ICMP echo to the router, 2 times");
    int pings = 0;
    for (uint16_t seq = 1; seq <= 2; seq++)
        pings += net_ping(nic, netRouter, routerMac, seq);

    kout_begin();
    e1000_dump_rings("rings after:  ");
    kout_end();
    netOk = (pings == 2);
    netDone = 1;
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED)
        halt();

    serial_init();
    boot_init();

    serial_puts("=== Lab 0x08 - PCI & NIC ===\n\n");

    pmm_init();                                   /* Bài 5 */
    paging_init();                                /* Bài 6 */
    gdt_init();                                   /* Bài 7 */
    acpi_init();                                  /* Bài 8: MADT */
    isr_init();                                   /* Bài 9 + 10: IDT, APIC, sti */
    timer_init();                                 /* Bài 10 */
    input_init();                                 /* = fsMount("/dev/") */
    initiateKb();                                 /* Bài 11 */
    initiateMouse();
    initiateTasks();                              /* Bài 12 */
    initiateKernelThreads();                      /* helper: reaper + helperNet */
    serial_puts("[boot] pmm, paging, GDT/TSS, ACPI, IDT, APIC, timer, PS/2, tasks ready\n\n");

    initiateNetworking();                         /* Bài 13: selectedNIC = 0 */
    uint64_t freeBefore = pmm_free_count();
    initiatePCI();                                /* Bài 14 */
    serial_puts("[pci] pmm frames used by PCI + NIC setup: ");
    serial_putdec(freeBefore - pmm_free_count());
    serial_puts(" (2 rings + 128 RX + 128 TX buffer frames)\n\n");

    if (!selectedNIC) {
        serial_puts("[kernel] no NIC found, nothing to send\n");
        halt();
    }

    Task *t = taskCreateKernel((uint64_t)netThread, 0);
    taskNameKernel(t, "net");

    while (!netDone)
        taskSleepMs(20);
    taskSleepMs(300);                             /* frame đến muộn (vd IPv6) vẫn được in */

    __asm__ volatile("cli");
    serial_puts("\n[kernel] E1000 interrupt handler ran ");
    serial_putdec(e1000Stats.irqs);
    serial_puts(" times (");
    serial_putdec(e1000Stats.irqsEmpty);
    serial_puts(" with ICR = 0). ICR causes seen: RXT0 ");
    serial_putdec(e1000Stats.rxt0);
    serial_puts(", RXDMT0 ");
    serial_putdec(e1000Stats.rxdmt);
    serial_puts(", RXO ");
    serial_putdec(e1000Stats.rxo);
    serial_puts(", TXDW ");
    serial_putdec(e1000Stats.txdw);
    serial_puts(", TXQE ");
    serial_putdec(e1000Stats.txqe);
    serial_puts(", LSC ");
    serial_putdec(e1000Stats.lsc);
    serial_puts("\n[kernel] frames: tx ");
    serial_putdec(e1000Stats.txFrames);
    serial_puts(", rx by IRQ ");
    serial_putdec(e1000Stats.rxFrames);
    serial_puts(", handled by helperNet ");
    serial_putdec(netRxCount);
    serial_puts(" (ignored by the lab: ");
    serial_putdec(netRxIgnored);
    serial_puts("), netQueue drops ");
    serial_putdec(netQueueDropped);
    serial_puts(netOk ? "\n[kernel] DHCP + ARP + 2 pings OK, cli; hlt\n" : "\n[kernel] demo FAILED, cli; hlt\n");
    halt();
}
