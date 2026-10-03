SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c \
          ps2.c input.c kb.c mouse.c
KERNEL := kernel.bin
IMG    := os.img

include ../common.mk

# Lab 0x06: scripts/ps2_input.py chạy QEMU có -monitor unix:... rồi gửi sendkey/mouse_move.
CFLAGS += $(DEMO)
