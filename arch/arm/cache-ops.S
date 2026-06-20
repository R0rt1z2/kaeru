
.syntax unified
.thumb
.text

.global arch_clean_invalidate_cache_range
.thumb_func
arch_clean_invalidate_cache_range:
    add             r2, r0, r1
    bic             r0, #(CONFIG_CACHE_LINE - 1)
0:
	mcr		p15, 0, r0, c7, c14, 1  /* clean & invalidate cache to PoC by MVA */
	add		r0, r0, #CONFIG_CACHE_LINE
	cmp		r0, r2
	blo		0b

	mov		r0, #0
	mcr		p15, 0, r0, c7, c10, 4  /* data sync barrier (formerly drain write buffer) */
	mov		r0, r4
	mov		r1, r5
	bx		lr

.global arch_clean_cache_range
.thumb_func
arch_clean_cache_range:
    add     r2, r0, r1
    bic     r0, r0, #(CONFIG_CACHE_LINE - 1)
0:
    mcr     p15, 0, r0, c7, c10, 1 /* clean cache to PoC by MVA */
    add     r0, r0, #CONFIG_CACHE_LINE
    cmp     r0, r2
    blo     0b
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4 /* data sync barrier (formerly drain write buffer) */
    bx      lr

.global arch_sync_cache_range
.thumb_func
arch_sync_cache_range:
    push    {lr}
    bl      arch_clean_invalidate_cache_range

    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0  /* invalidate entire icache */
    mcr     p15, 0, r0, c7, c10, 4 /* dsb */
    mcr     p15, 0, r0, c7, c5, 4  /* isb */

    pop     {pc}

.global enable_unaligned
.thumb_func
enable_unaligned:
    mrc     p15, 0, r1, c1, c0, 0
    and     r0, r1, #2
    bic     r1, r1, #2
    mcr     p15, 0, r1, c1, c0, 0
    dsb
    isb
    bx      lr

.global restore_unaligned
.thumb_func
restore_unaligned:
    mrc     p15, 0, r1, c1, c0, 0
    bic     r1, r1, #2
    orr     r1, r1, r0
    mcr     p15, 0, r1, c1, c0, 0
    dsb
    isb
    bx      lr

.section .note.GNU-stack, "", %progbits