SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c acpi.c apic.c timer.c \
          ps2.c input.c kb.c mouse.c \
          task.c schedule.c spinlock.c kernel_helper.c kout.c \
          pci.c nic.c e1000.c net.c ahci.c vfs.c syscall.c sse.c elf.c
KERNEL := kernel.bin
IMG    := os.img

# Lab 0x0c: như Lab 0x0b, thêm loader ELF; chương trình user build từ user/*.c thành ELF tĩnh
# và được scripts/mkimage_fs.sh chép vào /bin của phân vùng ext2.
QEMU_FLAGS ?= -M q35 -m 256M -serial stdio -no-reboot -no-shutdown -nic none -cpu max

include ../common.mk

# Chương trình ring 3: freestanding, không libc, ELF tĩnh ở 0x400000 (user/user.ld).
# Được dùng SSE (kernel thì không): Bài 17 đã bật và lưu FPU khi đổi task.
UCFLAGS := -std=gnu11 -O2 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-asynchronous-unwind-tables
ULDFLAGS := -nostdlib -static -no-pie -Wl,--build-id=none -T user/user.ld
USERBINS := userbin/hello

userbin/%: user/%.c user/ulib.h user/user.ld
	mkdir -p userbin
	$(CC) $(UCFLAGS) $< $(ULDFLAGS) -o $@

MKIMAGE     := scripts/mkimage_fs.sh
LIMINE_CONF := limine.conf
$(IMG): scripts/mkimage_fs.sh limine.conf $(USERBINS)

clean: clean-user
clean-user:
	rm -rf userbin

CFLAGS += $(DEMO)
