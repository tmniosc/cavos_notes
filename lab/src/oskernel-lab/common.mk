# common.mk — build chung cho mọi mini-lab trong ~/oskernel-lab
# Mỗi project chỉ cần khai: SRCS, KERNEL, IMG rồi `include ../common.mk`.
#
# Toolchain: ưu tiên cross x86_64-cavos-* (do cavOS `make tools` dựng ra).
# Nếu chưa có thì tự động lùi về gcc hệ thống — kernel freestanding vẫn build đúng.
# LƯU Ý: không bao giờ để comment cùng dòng với `VAR := value` (Make nuốt cả space vào giá trị).

CROSS_BIN   ?= $(HOME)/opt/cross/bin
CROSS_PREFIX ?= $(CROSS_BIN)/x86_64-cavos-

ifneq ($(wildcard $(CROSS_PREFIX)gcc),)
CC      := $(CROSS_PREFIX)gcc
OBJDUMP := $(CROSS_PREFIX)objdump
READELF := $(CROSS_PREFIX)readelf
NM      := $(CROSS_PREFIX)nm
TOOLCHAIN_KIND := cross ($(CROSS_PREFIX)gcc)
else
CC      := gcc
OBJDUMP := objdump
READELF := readelf
NM      := nm
TOOLCHAIN_KIND := host gcc (cross chưa build)
endif

KERNEL ?= kernel.bin
IMG    ?= os.img
SRCS   ?= kernel.c
OBJS   := $(SRCS:.c=.o)

BASE   := $(basename $(KERNEL))
MAP    := $(BASE).map
DIS    := $(BASE).dis
SYM    := $(BASE).sym
ELFTXT := $(BASE).elf.txt

LABROOT := $(dir $(lastword $(MAKEFILE_LIST)))

CFLAGS := -std=gnu11 -O2 -g -pipe -Wall -Wextra \
          -ffreestanding -fno-stack-protector -fno-stack-clash-protection \
          -fno-omit-frame-pointer -fno-pic -fno-lto \
          -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
          -mno-red-zone -mcmodel=kernel \
          -I$(LABROOT)

LDFLAGS := -nostdlib -static -no-pie -Wl,-z,max-page-size=0x1000 \
           -Wl,--build-id=none -Wl,-Map=$(MAP) -T linker.ld

QEMU       ?= qemu-system-x86_64
QEMU_FLAGS ?= -M q35 -m 256M -serial stdio -no-reboot -no-shutdown

LIMINE_DIR ?= $(HOME)/opt/limine
MKIMAGE    := $(LABROOT)scripts/mkimage.sh
LIMINE_CONF := $(LABROOT)limine.conf

.PHONY: all kernel image run run-gfx compdb clean distclean info

all: $(KERNEL)

kernel: $(KERNEL)

info:
	echo "toolchain : $(TOOLCHAIN_KIND)"
	echo "sources   : $(SRCS)"
	echo "kernel    : $(KERNEL)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(OBJDUMP) -d -M intel $@ > $(DIS)
	$(NM) -n $@ > $(SYM)
	$(READELF) -a $@ > $(ELFTXT)
	echo "[built] $@  (+ $(MAP) $(DIS) $(SYM) $(ELFTXT))"

image: $(IMG)

$(IMG): $(KERNEL) $(MKIMAGE) $(LIMINE_CONF)
	LIMINE_DIR="$(LIMINE_DIR)" CONF="$(LIMINE_CONF)" $(MKIMAGE) $(KERNEL) $(IMG)

run: $(IMG)
	$(QEMU) $(QEMU_FLAGS) -display none -drive file=$(IMG),format=raw,if=ide

run-gfx: $(IMG)
	$(QEMU) $(QEMU_FLAGS) -drive file=$(IMG),format=raw,if=ide

compdb:
	bear --output compile_commands.json -- $(MAKE) -B $(OBJS)

clean:
	rm -f $(OBJS) $(KERNEL) $(MAP) $(DIS) $(SYM) $(ELFTXT) $(IMG)

distclean: clean
	rm -f compile_commands.json
