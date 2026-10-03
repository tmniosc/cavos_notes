SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c \
          ps2.c input.c kb.c mouse.c \
          task.c schedule.c spinlock.c kernel_helper.c kout.c
KERNEL := kernel.bin
IMG    := os.img

include ../common.mk

# Lab 0x07: DEMO=-DNO_PRINT_LOCK | -DLOST_WAKEUP | -DHELPER_SPIN (xem kernel.c).
# scripts/tasks_input.py chạy QEMU có -monitor unix:... rồi gõ phím bằng sendkey.
CFLAGS += $(DEMO)
