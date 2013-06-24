.global GetStackHigh

.global UndefinedInstruction
.global SupervisorCall
.global PrefetchAbort
.global DataAbort
.global Irq
.global Fiq

.global _UndefinedInstruction
.global _SupervisorCall
.global _PrefetchAbort
.global _DataAbort
.global _Irq
.global _Fiq

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

stored_state_undef:
.word 0
.word 0

.func _UndefinedInstruction
_UndefinedInstruction:
	ldr sp, =stored_state_undef
	str lr, [sp]			/* need to subtract 4 for ARM, 2 for THUMB */
	mrs lr, SPSR
	str lr, [sp, #4]

	ldr sp, =initial_stack_end
	sub sp, #4

	push {r0-r12, r14}

	mov r0, #1
	blx GetStackHigh
	str sp, [r0]
	sub r0, #4
	mov sp, r0

	ldr r0, =stored_state_undef
	ldr r0, [r0]

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx UndefinedInstruction

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr sp, =stored_state_undef
	rfeia sp

.endfunc
.size _UndefinedInstruction, .-_UndefinedInstruction

stored_state_svc:
.word 0
.word 0

svc_return_value:
.word 0
.func _SupervisorCall
_SupervisorCall:
	ldr sp, =stored_state_svc
	str lr, [sp]			/* correctly returns to next instruction */
	mrs lr, SPSR
	str lr, [sp, #4]

	ldr sp, =initial_stack_end
	sub sp, #4

	push {r0-r12, r14}

	mov r0, #2
	blx GetStackHigh
	str sp, [r0]
	sub r0, #4
	mov sp, r0

	mov r0, r7

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx SupervisorCall
	ldr r3, =svc_return_value
	str r0, [r3]

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr r0, =svc_return_value
	ldr r0, [r0]

	ldr sp, =stored_state_svc
	rfeia sp

.endfunc
.size _SupervisorCall, .-_SupervisorCall

stored_state_pref:
.word 0
.word 0

.func _PrefetchAbort
_PrefetchAbort:
	ldr sp, =stored_state_pref
	#mrs lr, lr_abt
	str lr, [sp]			/* need to subtract 4, 0 for thumb */
	mrs lr, SPSR
	str lr, [sp, #4]

	ldr sp, =initial_stack_end
	sub sp, #4

	push {r0-r12, r14}

	mov r0, #4
	blx GetStackHigh
	str sp, [r0]
	sub r0, #4
	mov sp, r0

	ldr r0, =stored_state_pref
	ldr r0, [r0]

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx PrefetchAbort

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr sp, =stored_state_pref
	rfeia sp

.endfunc
.size _PrefetchAbort, .-_PrefetchAbort

stored_state_data:
.word 0
.word 0

.func _DataAbort
_DataAbort:
	ldr sp, =stored_state_data
	str lr, [sp]				/* need to subtract 8, 4 for thumb */
	mrs lr, SPSR
	str lr, [sp, #4]

	ldr sp, =initial_stack_end
	sub sp, #4

	push {r0-r12, r14}

	mov r0, #4
	blx GetStackHigh
	str sp, [r0]
	sub r0, #4
	mov sp, r0

	ldr r0, =stored_state_data
	ldr r0, [r0]

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx DataAbort

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr sp, =stored_state_data
	rfeia sp

.endfunc
.size _DataAbort, .-_DataAbort

stored_state_irq:
.word 0
.word 0

.func _Irq
_Irq:
	ldr sp, =stored_state_irq
	str lr, [sp]				/* need to subtract 4, 0 for thumb */
	mrs lr, SPSR
	str lr, [sp, #4]

	tst lr, #32					/* check for thumb */
	ldreq lr, [sp]
	subeq lr, #4
	streq lr, [sp]

	ldr sp, =initial_stack_end
	sub sp, #4

	push {r0-r12, r14}

	mov r0, #6
	blx GetStackHigh
	str sp, [r0]
	sub r0, #4
	mov sp, r0

	ldr r0, =stored_state_irq
	ldr r0, [r0]

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx Irq

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr sp, =stored_state_irq
	rfeia sp

.endfunc
.size _Irq, .-_Irq

.global master_clock
master_clock:
.word 0
.global pMasterClockClear
pMasterClockClear:
.word 0
.global masterClockClearValue
masterClockClearValue:
.word 0

stored_state_fiq:
.word 0
.word 0

.func _Fiq
_Fiq:
	ldr sp, =stored_state_fiq
	str lr, [sp]				/* need to subtract 4 in ARM, 0 for thumb */
	mrs lr, SPSR
	str lr, [sp, #4]

	tst lr, #32					/* check for thumb */
	ldreq lr, [sp]
	subeq lr, #4
	streq lr, [sp]

	ldr r8, =master_clock
	ldr r9, [r8]
	add r9, #1
	str r9, [r8]

	ldr r8, =pMasterClockClear
	ldr r8, [r8]
	ldr r9, =masterClockClearValue
	ldr r9, [r9]
	str r9, [r8]

	rfeia sp

.endfunc
.size _Fiq, .-_Fiq

.func ChangeModeAndJump
ChangeModeAndJump:
	ldr sp, [r3, #8]
	rfeia r3
.endfunc
.size ChangeModeAndJump, .-ChangeModeAndJump

.global EnableFpu
.func EnableFpu
EnableFpu:
	#get existing nsacr
	mrc p15, 0, r1, c1, c1, 2
	orr r1, #(3 << 10)
	#set it
	mcr p15, 0, r1, c1, c1, 2

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

