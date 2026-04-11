
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

.section .note.GNU-stack, "", %progbits