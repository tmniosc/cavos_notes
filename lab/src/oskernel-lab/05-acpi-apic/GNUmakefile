SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c
KERNEL := kernel.bin
IMG    := os.img

include ../common.mk

# DEMO=-DNO_VOLATILE: timerTicks không volatile như cavOS -> vòng chờ calibrate treo ở -O2
CFLAGS += $(DEMO)
