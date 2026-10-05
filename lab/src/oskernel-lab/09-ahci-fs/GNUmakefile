SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c \
          ps2.c input.c kb.c mouse.c \
          task.c schedule.c spinlock.c kernel_helper.c kout.c \
          pci.c nic.c e1000.c net.c ahci.c vfs.c
KERNEL := kernel.bin
IMG    := os.img

# Lab 0x09: ổ đĩa có 2 partition như disk.img của cavOS (partition 1 FAT32: Limine + kernel,
# partition 2 ext2: "/"). Trên máy q35, ổ if=ide của QEMU gắn vào controller AHCI ICH9 (00:1f.2).
# -nic none: không card mạng (lab này không cần).
QEMU_FLAGS ?= -M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none

include ../common.mk

# Dựng ổ bằng script riêng (gán sau include nên recipe của $(IMG) dùng giá trị mới)
MKIMAGE     := scripts/mkimage_fs.sh
LIMINE_CONF := limine.conf
$(IMG): scripts/mkimage_fs.sh limine.conf

# DEMO=-DPRDT_CAVOS (xem kernel.c, ahci.c).
CFLAGS += $(DEMO)
