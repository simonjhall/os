.section .entry, "ax"

.global Setup
.global _start
.global entry
.global initial_stack
.global initial_stack_end

.global __bss_start__
.global __bss_end__

.global GetStackHigh

.func entry
entry:
	#clear bss with memset (as it doesn't use the stack)
	ldr r0, =__bss_start__
	ldr r2, =__bss_end__
	sub r2, r0
	mov r1, #0
	bl memset

	cps #31
	ldr sp, =initial_stack
	ldr r0, =initial_stack_end
	mov sp, r0
	sub sp, #4

	mov r0, #0
	blx GetStackHigh
	mov sp, r0

	blx Setup
done:
	b done
.endfunc
.size entry, .-entry

.section .trampoline, "ax"

.global __trampoline_start__
__trampoline_start__:

.global __UndefinedInstruction
.global __SupervisorCall
.global __PrefetchAbort
.global __DataAbort
.global __Irq
.global __Fiq

.global __UndefinedInstruction_addr
__UndefinedInstruction_addr:
.word 0

.global __SupervisorCall_addr
__SupervisorCall_addr:
.word 0

.global __PrefetchAbort_addr
__PrefetchAbort_addr:
.word 0

.global __DataAbort_addr
__DataAbort_addr:
.word 0

.global __Irq_addr
__Irq_addr:
.word 0

.global __Fiq_addr
__Fiq_addr:
.word 0

__UndefinedInstruction:
	ldr sp, =__UndefinedInstruction_addr
	ldr sp, [sp]
	bx sp
__SupervisorCall:
	ldr sp, =__SupervisorCall_addr
	ldr sp, [sp]
	bx sp
__PrefetchAbort:
	ldr sp, =__PrefetchAbort_addr
	ldr sp, [sp]
	bx sp
__DataAbort:
	ldr sp, =__DataAbort_addr
	ldr sp, [sp]
	bx sp
__Irq:
	ldr sp, =__Irq_addr
	ldr sp, [sp]
	bx sp
__Fiq:
	ldr sp, =__Fiq_addr
	ldr sp, [sp]
	bx sp

.global __trampoline_end__
__trampoline_end__:
