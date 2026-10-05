SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c           ps2.c input.c kb.c mouse.c           task.c schedule.c spinlock.c kernel_helper.c kout.c           pci.c nic.c e1000.c net.c
KERNEL := kernel.bin
IMG    := os.img

# Lab 0x08: card e1000 nối vào mạng "user" của QEMU (slirp: DHCP server + router 10.0.2.2),
# y như cavOS "make qemu" (-netdev user,id=mynet0 -net nic,model=e1000,netdev=mynet0).
# filter-dump ghi mọi frame đi qua netdev vào PCAP (đọc bằng scripts/pcap_summary.py).
PCAP ?= lab08.pcap
QEMU_FLAGS ?= -M q35 -m 256M -serial stdio -no-reboot -no-shutdown               -netdev user,id=mynet0 -net nic,model=e1000,netdev=mynet0               -object filter-dump,id=f0,netdev=mynet0,file=$(PCAP)

include ../common.mk

# DEMO=-DEOI_LATE | -DWAIT_RCTL | -DPCI_ACTIVE_LOW (xem kernel.c).
CFLAGS += $(DEMO)
