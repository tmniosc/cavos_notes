SRCS   := kernel.c boot.c serial.c pmm.c paging.c gdt.c idt.c isr.c
KERNEL := kernel.bin
IMG    := os.img

include ../common.mk

# DEMO=-DDF_DEMO (stack hỏng -> #DF), thêm -DNO_IST để thấy như cavOS (không IST)
CFLAGS += $(DEMO)
