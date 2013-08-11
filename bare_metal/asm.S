.global InvokeSyscall
.global ChangeModeAndJump

.global EnableIrq
.global EnableFiq
.global EnableIrqFiqForMode

.func InvokeSyscall
InvokeSyscall:
	push {r7}
	mov r7, r0
	swi 0
	pop {r7}
	bx lr
.endfunc

.func ChangeModeAndJump
ChangeModeAndJump:
	ldr sp, [r3, #8]
	rfeia r3
.endfunc
.size ChangeModeAndJump, .-ChangeModeAndJump

.global EnableFpu
.func EnableFpu
EnableFpu:
	/*
	#unreadable in qemu, unwritable in non-secure...why bother
	#get existing nsacr
	mrc p15, 0, r1, c1, c1, 2
	orr r1, #(3 << 10)
	#set it
	mcr p15, 0, r1, c1, c1, 2*/

	#get existing cpacr
	mrc p15, 0, r1, c1, c0, 2
	#enable cp 11 and 10
	mov r0, #(0xf << 20)
	orr r1, r0
	#set it back
	mcr p15, 0, r1, c1, c0, 2

	#get the existing fpexc
	vmrs r1, FPEXC
	orr r1, #(1 << 30)
	vmsr FPEXC, r1

	bx lr
.endfunc
.size EnableFpu, .-EnableFpu

.global FlushTlb
.func FlushTlb
FlushTlb:
	#ensure table writes are visible
	dsb
	#todo insert cache flush

	#flush all tlb
	mcr p15, 0, r0, c8, c7, 0

	#ensure flush is finished
	dsb

	#get instruction fetch working ok
	isb

	bx lr
.endfunc
.size FlushTlb, .-FlushTlb

.func EnableIrq
EnableIrq:
	cmp r0, #0
	beq 1f

	/* enable */
	cpsie i
	bx lr
1:
	/* disable */
	cpsid i
	bx lr
.endfunc
.size EnableIrq, .-EnableIrq

.func EnableFiq
EnableFiq:
	cmp r0, #0
	beq 1f

	/* enable */
	cpsie f
	bx lr
1:
	/* disable */
	cpsid f
	bx lr
.endfunc
.size EnableFiq, .-EnableFiq

/* ChangeToMode(mode) */
.func ChangeToMode
ChangeToMode:
	cmp r0, #17		/* fiq */
	beq 1f
	cmp r0, #18		/* irq */
	beq 2f
	cmp r0, #19		/* supervisor */
	beq 3f
	cmp r0, #23		/* abort */
	beq 4f
	cmp r0, #27		/* undef */
	beq 5f
	cmp r0, #31		/* system */
	beq 6f

	#fail here
	mov r0, #0
	bx lr
1:
	/* keep the link register before the mode switch */
	mov r0, lr
	/* change mode */
	cps #17
	bx r0
2:
	mov r0, lr
	cps #18
	bx r0
3:
	mov r0, lr
	cps #19
	bx r0
4:
	mov r0, lr
	cps #23
	bx r0
5:
	mov r0, lr
	cps #27
	bx r0
6:
	mov r0, lr
	cps #31
	bx r0
.endfunc
.size ChangeToMode, .-ChangeToMode

/* EnableIrqFiqForMode(mode, irq, fiq) */
.func EnableIrqFiqForMode
EnableIrqFiqForMode:
	push {lr}

	/* get current mode */
	mrs r3, cpsr
	and r3, #31

	/* change to new mode */
	bl ChangeToMode

	/* handle for failure */
	cmp r0, #0
	moveq r0, #0
	popeq {pc}

	/* enable interrupts */
	mov r0, r1
	bl EnableIrq

	mov r0, r2
	bl EnableFiq

	/* return to the old mode */
	mov r0, r3
	bl ChangeToMode

	mov r0, #1
	pop {pc}
.endfunc
.size EnableIrqFiqForMode, .-EnableIrqFiqForMode

.global InsertPageTableLoAndFlush
.func InsertPageTableLoAndFlush
InsertPageTableLoAndFlush:
	push {lr}
	push {r0}

	bl v7_flush_icache_all
	bl v7_flush_dcache_all

	pop {r0}

	mcr p15, 0, r0, c2, c0, 0

	bl FlushTlb

	pop {pc}
.endfunc
.size InsertPageTableLoAndFlush, .-InsertPageTableLoAndFlush

.global OmapSmc
.func OmapSmc
OmapSmc:
	push {r4-r12, lr}

	#the thing to call
	mov r12, r2

	#r0
	mov r2, r0
	ldr r0, [r2]
	#r1
	mov r3, r1
	ldr r1, [r3]

	#call it
	push {r2, r3}
	smc 1
	pop {r2, r3}

	#r0
	str r0, [r2]
	#r1
	str r1, [r3]


	pop {r4-r12, pc}
.endfunc
.size OmapSmc, .-OmapSmc

.global DelaySecond
.func DelaySecond
DelaySecond:
	ldr r0, =333333333
1:
	subs r0, #1
	bne 1b
	bx lr
.endfunc
.size DelaySecond, .-DelaySecond

.global DelayMillisecond
.func DelayMillisecond
DelayMillisecond:
	ldr r0, =333333
1:
	subs r0, #1
	bne 1b
	bx lr
.endfunc
.size DelayMillisecond, .-DelayMillisecond


/***************************************/

.global TlsLow
.global TlsHigh

TlsLow:
.word 0
.func GetTls
GetTls:
	ldr r0, [pc, #-12]
	bx lr
.endfunc
.size GetTls, .-GetTls
TlsHigh:

