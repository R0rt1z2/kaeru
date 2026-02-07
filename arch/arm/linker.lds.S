OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(main)

SECTIONS
{
    .text :
    {
        *(.text.start)
        *(.text.main)
        *(.text .text.* .gnu.linkonce.t.*)
    }

    .rodata :
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    .got :
    {
        __got_start = .;
        *(.got .got.*)
        __got_end = .;
    }

    .data :
    {
        *(.data .data.* .gnu.linkonce.d.*)
    }

    .bss :
    {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.interp)
        *(.dynsym)
        *(.dynstr)
        *(.hash)
        *(.dynamic)
        *(.comment)
    }
}