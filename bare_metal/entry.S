.section .entry, "ax"

.global Setup
.global _start
.global entry
.global initial_stack
.global initial_stack_end

.global GetStackHigh

.func entry
entry:
	cps #31
	ldr sp, =initial_stack
	ldr r0, =initial_stack_end
	mov sp, r0
	sub sp, #4

	mov r0, #0
	blx GetStackHigh
	mov sp, r0

	blx Setup

	blx _start
done:
	b done
.endfunc
.size entry, .-entry
