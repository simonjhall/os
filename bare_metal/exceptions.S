.if M3==0

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

	#clear any lock
	clrex

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

.global \storeName
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
	#clear any exclusive lock
	clrex

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
