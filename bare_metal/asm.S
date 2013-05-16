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

.func InvokeSyscall
InvokeSyscall:
	push {r7}
	mov r7, r0
	swi 0
	pop {r7}
	bx lr
.endfunc

.global stored_state
stored_state:
.word 0
.word 0

.func _UndefinedInstruction
_UndefinedInstruction:
	ldr sp, =stored_state
	str lr, [sp]
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

	ldr r0, =stored_state
	ldr r0, [r0]

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx UndefinedInstruction

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr sp, =stored_state
	rfeia sp

.endfunc
.size _UndefinedInstruction, .-_UndefinedInstruction

svc_return_value:
.word 0
.func _SupervisorCall
_SupervisorCall:
	ldr sp, =stored_state
	str lr, [sp]
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

	ldr sp, =stored_state
	rfeia sp

.endfunc
.size _SupervisorCall, .-_SupervisorCall

.func _PrefetchAbort
_PrefetchAbort:
	ldr sp, =stored_state
	#mrs lr, lr_abt
	str lr, [sp]
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

	ldr r0, =stored_state
	ldr r0, [r0]

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx PrefetchAbort

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr sp, =stored_state
	rfeia sp

.endfunc
.size _PrefetchAbort, .-_PrefetchAbort

.func _DataAbort
_DataAbort:
	ldr sp, =stored_state
	str lr, [sp]
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

	ldr r0, =stored_state
	ldr r0, [r0]

	ldr r1, =initial_stack_end
	sub r1, #((14 + 1) * 4)

	blx DataAbort

	add sp, #4
	ldr sp, [sp]

	pop {r0-r12, r14}

	ldr sp, =stored_state
	rfeia sp

.endfunc
.size _DataAbort, .-_DataAbort

.func ChangeModeAndJump
ChangeModeAndJump:
	ldr sp, [r3, #8]
	rfeia r3
.endfunc
.size ChangeModeAndJump, .-ChangeModeAndJump

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

