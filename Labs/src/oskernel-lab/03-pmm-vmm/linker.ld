/* linker.ld — kernel higher-half cho Limine (base revision 2).
 * ENTRY = kmain (lab dùng kmain làm entry, không phải _start như cavOS).
 * Không ghi FLAGS trong PHDRS -> ld tự suy W^X từ section flags (AX / A / WA).
 */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    text    PT_LOAD;
    rodata  PT_LOAD;
    data    PT_LOAD;
}

SECTIONS
{
    /* top-2GiB của higher-half: nơi Limine map kernel image */
    . = 0xffffffff80000000;

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        /* KEEP: không ai tham chiếu trực tiếp -> chống linker GC bỏ đi */
        KEEP(*(.limine_requests))
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}
