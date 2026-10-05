/* user.ld — chương trình ring 3 của Lab 0x0c: ELF tĩnh, nạp ở 4 MiB như một chương trình Linux
 * không PIE. Đoạn đầu chứa luôn ELF header + program header (AT_PHDR trỏ vào đó); .data/.bss
 * sang trang riêng để có quyền ghi riêng. */
ENTRY(_start)
PHDRS {
    text PT_LOAD FILEHDR PHDRS FLAGS(5);   /* R X */
    data PT_LOAD FLAGS(6);                 /* R W */
}
SECTIONS {
    . = 0x400000 + SIZEOF_HEADERS;
    .text   : { *(.text .text.*) } :text
    .rodata : { *(.rodata .rodata.*) } :text
    . = ALIGN(0x1000);
    .data   : { *(.data .data.*) } :data
    .bss    : { *(.bss .bss.* COMMON) } :data
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
