.syntax unified
.thumb

.section .text.start, "ax"
.global main
.thumb_func
main:
    push    {r4, lr}

    adr     r0, 1f
    ldr     r1, =1f
    subs    r4, r0, r1
    beq     .Lno_reloc

    ldr     r0, =__got_start
    ldr     r1, =__got_end
    adds    r0, r0, r4
    adds    r1, r1, r4

.Lreloc_loop:
    cmp     r0, r1
    bge     .Lreloc_done
    ldr     r2, [r0]
    cbz     r2, .Lreloc_skip
    adds    r2, r2, r4
    str     r2, [r0]
.Lreloc_skip:
    adds    r0, #4
    b       .Lreloc_loop

.Lreloc_done:
    ldr     r0, =__got_start
    ldr     r1, =__got_end
    adds    r0, r0, r4
    subs    r1, r1, r0
    bl      arch_clean_invalidate_cache_range

.Lno_reloc:
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    movs    r2, #0
.Lbss_loop:
    cmp     r0, r1
    bge     .Lbss_done
    str     r2, [r0]
    adds    r0, #4
    b       .Lbss_loop
.Lbss_done:
    bl      kaeru_early_init

    pop     {r4, pc}

    .align 2
1:  .word 0

.section .note.GNU-stack, "", %progbits