SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c \
          ps2.c input.c kb.c mouse.c \
          task.c schedule.c spinlock.c kernel_helper.c kout.c \
          pci.c nic.c e1000.c net.c ahci.c vfs.c syscall.c
KERNEL := kernel.bin
IMG    := os.img

# Lab 0x0a: như Lab 0x09 (ổ FAT32 + ext2 trên AHCI), thêm SYSCALL/SYSRET và 3 chương trình ring 3.
QEMU_FLAGS ?= -M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none

include ../common.mk

# user.S: chương trình ring 3 viết bằng asm (common.mk chỉ có luật cho .c)
OBJS += user.o
$(KERNEL): user.o
user.o: user.S
	$(CC) $(CFLAGS) -c $< -o $@

MKIMAGE     := scripts/mkimage_fs.sh
LIMINE_CONF := limine.conf
$(IMG): scripts/mkimage_fs.sh limine.conf

# DEMO=-DBAD_SYSRET (xem kernel.c, user.S).
CFLAGS += $(DEMO)
