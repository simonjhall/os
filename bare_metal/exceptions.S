.if M3==0

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

	#clear any lock
	clrex

	#change to a mode with banked registers and spsr (supervisor)
	cps #19

	#load the spsr
	ldr r1, [r0, #4]
	msr spsr, r1

	#reload all the registers and pc, switch in cpsr
	add r0, #8
	mov sp, r0
	#load r0-r14
	ldm sp, {r0-r14}^
	#move up to d0
	add sp, #(15 * 4)
	vldmia sp!, {q0-q7}
	vldmia sp!, {q8-q15}
	#load the tpidruro
	ldr lr, [sp], #4
	mcr p15, 0, lr, c13, c0, 3
	#reload pc
	ldm sp, {r15}^
.endfunc
.size Resume, .-Resume

.macro STORAGE
	#return address, spsr
	.fill 2, 4, 0
	#r0-r12
	.fill 13, 4, 0
	#sp/13: usr/sys
	.fill 1, 4, 0
	#lr/14
	.fill 1, 4, 0
	#D0-D31
	.fill 32, 8, 0
	#tpidruro
	.fill 1, 4, 0
	#target pc
	.fill 1, 4, 0
	#exception type
	.fill 1, 4, 0
	#dfar, dfsr
	.fill 2, 4, 0
.endm

.macro HANDLER exceptionId, funcName, handlerFunc, storeName

.global \storeName
\storeName:
	STORAGE
	STORAGE
	/* will end up in system mode */
.global \funcName
.func \funcName
\funcName:
	#clear any exclusive lock
	clrex

	#IRQ is disable for all of this
	#FIQ may still be enabled

	/* point to our state
	this will trash sp assuming we came from the same mode
	only irq is an async event - which is turned off
	could have a memory failure though, from data or pref abt
	may have come from fiq...which would be fatal */

	#get the cpu id, 0 or 1
	mrc p15, 0, sp, c0, c0, 5
	and sp, #1
	lsl sp, #3			//0=0, 1=8

	add pc, pc, sp		//0
	nop				//4
	ldr sp, =\storeName	//8		entry for cpu zero
	b 1f				//c
	ldr sp, =\storeName	//10		entry for cpu one
	add sp, #(86*4)		//14

1:
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

	stmia r0!, {sp, lr}

	#store all adv simd
	vstmia r0!, {q0-q7}
	vstmia r0!, {q8-q15}

	#store tpidruro
	mrc p15, 0, r1, c13, c0, 3
	str r1, [r0]

	#which cpu are we
	mrc p15, 0, r1, c0, c0, 5
	ands r1, #1					/* only support cpu 0 and 1 */

	#get our proper stack
	#everything is disabled here - except for fiq
	ldreq sp, =exception_stack_end
	ldrne sp, =exception_stack_cpu2_end
	ldr sp, [sp]
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

.global HandlerYield
.func HandlerYield
 HandlerYield:
 	push {r7}
 	mov r7, #158
	swi #0
	mov r0, r1
	pop {r7}
	bx lr
.endfunc
.size  HandlerYield, .- HandlerYield
.endif
