.section .entry, "ax"

.global Setup
.global _start
.global entry
.global initial_stack
.global initial_stack_end

.global __bss_start__
.global __bss_end__

.global GetStackHigh

.global master_tlb

#16kb aligned
.align 14
entry:
master_tlb:
	bl phys_entry
	#fill it with fault entries
	.fill 4095, 4, 0xfffffff0

.func phys_entry
phys_entry:
	#we almost have our phys load point
	sub lr, #4
	#clear the first entry
	mov r0, #0xfffffff0
	str r0, [lr]

	ldr r0, =entry
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
	mov r4, #0x1e00				/* tex=1 | ap=3 | one | domain=0000| xn=0 | c=1 | b=1 | one | zero*/
	orr r4, #0xe

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

	#insert the page table
	mcr p15, 0, lr, c2, c0, 0

	#change the domain to client
	mov r2, #1
	mcr p15, 0, r2, c3, c0, 0

	#get ready to jump
	ldr r3, =virt_entry

	#turn on the mmu
	#get the current value
	mrc p15, 0, r2, c1, c0, 0
	orr r2, #1					/* enable */
	mcr p15, 0, r2, c1, c0, 0	/* turn on*/

	#keep the load addr
	mov r4, lr
	#virt entry
	bx r3

virt_entry:
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

	mov r0, r4
	blx Setup
done:
	b done
fail:
	b fail
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
