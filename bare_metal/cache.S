.if M3==0

/* http://lxr.free-electrons.com/source/arch/arm/mm/cache-v7.S?a=arm */
/* https://github.com/torvalds/linux/blob/master/arch/arm/mm/cache-v7.S */

.global v7_invalidate_l1
.func v7_invalidate_l1
 v7_invalidate_l1:
 	push {r4, r5, r6}

 	mov     r0, #0
 	mcr     p15, 2, r0, c0, c0, 0
 	mrc     p15, 1, r0, c0, c0, 0

 	ldr     r1, =0x7fff
	and     r2, r1, r0, lsr #13

 	ldr     r1, =0x3ff

	and     r3, r1, r0, lsr #3      @ NumWays - 1
 	add     r2, r2, #1              @ NumSets

	and     r0, r0, #0x7
 	add     r0, r0, #4      @ SetShift

	clz     r1, r3          @ WayShift
 	add     r4, r3, #1      @ NumWays
1:	sub     r2, r2, #1      @ NumSets--
	mov     r3, r4          @ Temp = NumWays
2:	subs    r3, r3, #1      @ Temp--
 	mov     r5, r3, lsl r1
 	mov     r6, r2, lsl r0
 	orr     r5, r5, r6      @ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
 	mcr     p15, 0, r5, c7, c6, 2
 	bgt     2b
 	cmp     r2, #0
 	bgt     1b
 	dsb
 	isb

 	pop {r4, r5, r6}
 	mov     pc, lr
.endfunc
.size v7_invalidate_l1, .-v7_invalidate_l1

.global v7_flush_icache_all
.func v7_flush_icache_all
v7_flush_icache_all:
	mov     r0, #0
	mcr      p15, 0, r0, c7, c5, 0          @ I+BTB cache invalidate
	mov     pc, lr
.endfunc
.size v7_flush_icache_all, .-v7_flush_icache_all

.global v7_flush_dcache_all
.func v7_flush_dcache_all
v7_flush_dcache_all:
	push {r4, r5, r6, r7, r9, r10, r11}

	dmb					@ ensure ordering with previous memory accesses
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr
	ands	r3, r0, #0x7000000		@ extract loc from clidr
	mov	r3, r3, lsr #23			@ left align loc bit field
	beq	finished			@ if loc is 0, then no need to clean
	mov	r10, #0				@ start clean at cache level 0
flush_levels:
	add	r2, r10, r10, lsr #1		@ work out 3x current cache level
	mov	r1, r0, lsr r2			@ extract cache type bits from clidr
	and	r1, r1, #7			@ mask of the bits for current cache only
	cmp	r1, #2				@ see what cache we have at this level
	blt	skip				@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	isb					@ isb to sych the new cssr&csidr
	mrc	p15, 1, r1, c0, c0, 0		@ read the new csidr
	and	r2, r1, #7			@ extract the length of the cache lines
	add	r2, r2, #4			@ add 4 (line length offset)
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3		@ find maximum number on the way size
	clz	r5, r4				@ find bit position of way size increment
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13		@ extract max number of the index size
loop1:
	mov	r9, r4				@ create working copy of max way size
loop2:
	orr	r11, r10, r9, lsl r5		@ factor way and cache number into r11
	orr	r11, r11, r7, lsl r2		@ factor index number into r11
	mcr	p15, 0, r11, c7, c14, 2		@ clean & invalidate by set/way
	subs	r9, r9, #1			@ decrement the way
	bge	loop2
	subs	r7, r7, #1			@ decrement the index
	bge	loop1
skip:
	add	r10, r10, #2			@ increment cache number
	cmp	r3, r10
	bgt	flush_levels
finished:
	mov	r10, #0				@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	dsb
	isb

	pop {r4, r5, r6, r7, r9, r10, r11}
	mov	pc, lr
.endfunc
.size v7_flush_dcache_all, .-v7_flush_dcache_all



/**********************/
.global EnableCaches
.func EnableCaches
EnableCaches:
	mrc p15, 0, r2, c1, c0, 0
	orr r2, #(1 << 12)			/* icache enable */
	orr r2, #(1 << 11)			/* branch prediction enable */
	orr r2, #(1 << 2)				/* dcache enable */
	mcr p15, 0, r2, c1, c0, 0		/* turn on*/

	bx lr

.endif
