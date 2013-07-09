.global GetStackHigh

.global UndefinedInstruction
.global SupervisorCall
.global PrefetchAbort
.global DataAbort
.global Irq
.global Fiq

.global Resume

.global _UndefinedInstruction
.global _SupervisorCall
.global _PrefetchAbort
.global _DataAbort
.global _Irq
.global _Fiq

/* r0=structure from below
turns off interrupts too */

.func Resume
Resume:
	#turn off interrupts
	cpsid i
	cpsid f

	#change to a mode with banked registers and spsr (supervisor)
	cps #19

	#load the spsr
	ldr r1, [r0, #4]
	msr spsr, r1

	#reload all the registers and pc, switch in cpsr
	add r0, #8
	#ldm r0, {r0-r15}^
	mov sp, r0
	ldm sp, {r0-r14}^
	add sp, #(15 * 4)
	ldm sp, {r15}^
.endfunc
.size Resume, .-Resume

.macro HANDLER exceptionId, funcName, handlerFunc, storeName

\storeName:
	#return address, spsr
	.fill 2, 4, 0
	#r0-r12
	.fill 13, 4, 0
	#sp/13: usr/sys
	.fill 1, 4, 0
	#lr/14
	.fill 1, 4, 0
	#target pc
	.fill 1, 4, 0
	#exception type
	.fill 1, 4, 0

	/* will end up in system mode */
.global \funcName
.func \funcName
\funcName:
	#IRQ is disable for all of this
	#FIQ may still be enabled

	/* point to our state
	this will trash sp assuming we came from the same mode
	only irq is an async event - which is turned off
	could have a memory failure though, from data or pref abt
	may have come from fiq...which would be fatal */
	ldr sp, =\storeName

	#store to-be-adjusted return address
	#see page B1-1171 for offsets
	str lr, [sp], #4

	#store old cpsr
	mrs lr, spsr
	str lr, [sp], #4

	#store registers
	stm sp!, {r0-r12}

	#store usr/sys sp/lr by changing to its mode (where we will stay)
	mov r0, sp			/* keep a hold of the old output pointer for convenience */
	cps #31			/* system, real sp and lr are live */

	stm r0, {sp, lr}

	#get our proper stack
	#everything is disabled here - except for fiq
	ldr sp, =initial_stack_end
	sub sp, #8

	#we now have a proper stack for the interrupt category
	#set up the arguments for the function
	ldr r0, =\storeName
	mov r1, #\exceptionId

	#call the function
	blx \handlerFunc
	#shoud never reach here
	bkpt
.endfunc
.size \funcName, .-\funcName

.endm

.global NewHandler

HANDLER 1, _Und, NewHandler, __UndState
HANDLER 2, _Svc, NewHandler, __SvcState
HANDLER 3, _Prf, NewHandler, __PrfState
HANDLER 4, _Dat, NewHandler, __DatState
HANDLER 6, _Irq, NewHandler, __IrqState

.if 0
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

.endif

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
	sub lr, #4
	str lr, [sp]				/* need to subtract 4  */
	mrs lr, SPSR
	str lr, [sp, #4]

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


