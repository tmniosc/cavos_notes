SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c \
          ps2.c input.c kb.c mouse.c \
          task.c schedule.c spinlock.c kernel_helper.c kout.c \
          pci.c nic.c e1000.c net.c ahci.c vfs.c syscall.c sse.c
KERNEL := kernel.bin
IMG    := os.img

# Lab 0x0b: như Lab 0x0a, thêm initiateSSE() và lưu FPU khi đổi task. -cpu max: TCG có AVX.
QEMU_FLAGS ?= -M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none -cpu max

include ../common.mk

# user.S: chương trình ring 3 viết bằng asm (common.mk chỉ có luật cho .c)
OBJS += user.o
$(KERNEL): user.o
user.o: user.S
	$(CC) $(CFLAGS) -c $< -o $@

MKIMAGE     := scripts/mkimage_fs.sh
LIMINE_CONF := limine.conf
$(IMG): scripts/mkimage_fs.sh limine.conf

# DEMO=-DXSAVE | -DNO_FPU_SAVE (xem schedule.c).
CFLAGS += $(DEMO)
