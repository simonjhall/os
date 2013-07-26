.section .entry, "ax"

.global Setup
.global _start
.global entry_maybe_misaligned
.global initial_stack
.global initial_stack_end

.global __bss_start__
.global __bss_end__

.global GetStackHigh

.global master_tlb

.macro PRINT_NEWLINE from
	mov r10, #0
	movt r10, #0x4802

	mov r11, #'\r'
	str r11, [r10]

	mov r12, lr
	bl wait
	mov lr, r12


	mov r11, #'\n'
	str r11, [r10]

	mov r12, lr
	bl wait
	mov lr, r12
.endm

.macro PRINT from
	mov r10, #0
	movt r10, #0x4802

	mov r9, \from

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	mov r12, lr
	bl wait
	mov lr, r12

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	mov r12, lr
	bl wait
	mov lr, r12

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	mov r12, lr
	bl wait
	mov lr, r12

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	mov r12, lr
	bl wait
	mov lr, r12

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	mov r12, lr
	bl wait
	mov lr, r12

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	mov r12, lr
	bl wait
	mov lr, r12

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	mov r12, lr
	bl wait
	mov lr, r12

	and r11, r9, #0xf0000000
	lsr r11, #28
	cmp r11, #10
	addlt r11, #'0'
	subge r11, #10
	addge r11, #'a'
	str r11, [r10]
	lsl r9, #4

	PRINT_NEWLINE
.endm

#16kb aligned
#pbes disable
.if PBES==0
.align 14
.endif

entry_maybe_misaligned:
master_tlb:
	bl phys_entry
	#fill it with fault entries
	.if PBES==1
	#pbes
	.fill 4079, 4, 0xfffffff0
	.else
	.fill 4095, 4, 0xfffffff0
	.endif

.func phys_entry
phys_entry:
	#we almost have our phys load point
	sub lr, #4
	.if PBES==1
	#pbes
	sub lr, #64
	.endif
	#clear the first entry
	mov r0, #0xfffffff0
	str r0, [lr]

	ldr r0, =entry_maybe_misaligned
	.if PBES==1
	#pbes
	sub r0, #64
	.endif
	ldr r1, =__end__

	#round the end up to the megabyte
	mov r2, #1048576
	sub r2, #1
	add r1, r2
	bic r1, r2

	#compute the length
	sub r1, r1, r0

	#r0 is the virtual addr of where we want it
	#r1 length of the image to map
	#lr is the start of the image to map

	#round the length up to the megabyte
	add r1, r2
	bic r1, r2

	#round virt down to megabytes, r0=virt start, r2=virt start MB
	lsr r2, r0, #20
	#keep the master tlb address in lr, phys MB in r3
	lsr r3, lr, #20
	#make a loop count from length
	lsrs r1, #20

	#should be nothing to map
	beq fail

map_loop:
	#construct a section entry for the phys address
	#but store twice, once to the target virtual address and one to the physical address
	mov r4, #0x1600				/* tex=1 | ap=1 | one | domain=0000| xn=0 | c=0 | b=0 | one | zero*/
	orr r4, #0x2

	#phys addr of the section
	lsl r5, r3, #20
	orr r4, r5

	#phys mapping
	str r4, [lr, r3, lsl #2]
	#virt mapping
	str r4, [lr, r2, lsl #2]

	#increment phys/virt MB
	add r3, #1
	add r2, #1

	subs r1, #1
	bne map_loop

.if 0
	#add the uart for debugging
	mov r2, #0x480
	lsl r2, #20

	orr r2, #0x2e00			/* tex=2 | ap=3 | one | domain=0000| xn=0 | c=0 | b=0 | one | zero*/
	orr r2, #0x2

	str r2, [lr, r2, lsr #18]
	mov r2, #0x480
	ldr r2, [lr, r2, lsl #2]
.endif

	#insert the page table into ttbr0
	mcr p15, 0, lr, c2, c0, 0
	mcr p15, 0, lr, c2, c0, 1		/* and ttbr1 */

	#change the domain to client
	mov r2, #1
	mcr p15, 0, r2, c3, c0, 0

	#get ready to jump
	ldr r3, =virt_entry

	#ensure the write has gone through
	dsb

	#set ttbr0 and ttbr1 to have an even 2GB split
	mrc p15, 0, r2, c2, c0, 2
	orr r2, #1
	mcr p15, 0, r2, c2, c0, 2

	#turn on the mmu (sctlr)
	#get the current value
	mrc p15, 0, r2, c1, c0, 0
	orr r2, #1					/* enable */
	bic r2, #2					/* disable alignment fault checking */
	orr r2, #(1 << 13)			/* remap high exception vector */
	mcr p15, 0, r2, c1, c0, 0		/* turn on*/

	#good to go
	dsb
	isb

	#keep the load addr
	mov r4, lr
	#virt entry
	bx r3

virt_entry:
	#clear bs
	ldr r0, =__bss_start__
	ldr r2, =__bss_end__
	subs r2, r0
	mov r1, #0
	beq done_clear

bss_clear:
	subs r2, #1
	strb r1, [r0], #1
	bne bss_clear
done_clear:

	cps #31
	ldr sp, =initial_stack
	ldr r0, =initial_stack_end
	ldr r0, [r0]
	mov sp, r0
	sub sp, #8

	mov r0, #0
	blx GetStackHigh
	mov sp, r0

	mov r0, r4
	blx Setup
done:
	b done
fail:
	b fail
wait:
	mov r13, #0xf000
wait_loop:
	subs r13, #1
	bne wait_loop
	bx lr
.endfunc
.size entry_maybe_misaligned, .-entry_maybe_misaligned

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
