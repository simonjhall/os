.section .entry, "ax"

.if M3==0

.global Setup
.global _start
.global entry_maybe_misaligned
/*.global initial_stack
.global initial_stack_end
.global initial_stack_cpu2
.global initial_stack_cpu2_end*/

.global __bss_start__
.global __bss_end__

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
	ldr sp, =initial_stack_end
	ldr sp, [sp]
	sub sp, #8

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

.global secondary_entry
secondary_entry:
.func secondary_entry
	#get the current PA pc
	bl 1f
1:
	sub lr, #4

	#get the VA->PA offset
	ldr r0, =secondary_entry
	sub r0, lr

	#compute the address of the master page table
	ldr r1, =master_tlb
	sub r1, r0

	#round it down to the megabyte
	mov r0, #1048576
	sub r0, #1
	bic r1, r0

	#insert the page table into ttbr0 and 1
	mcr p15, 0, r1, c2, c0, 0
	mcr p15, 0, r1, c2, c0, 1

	#change the domain to client
	mov r2, #1
	mcr p15, 0, r2, c3, c0, 0

	#get ready to jump
	ldr r3, =virt_entry_2

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

	bx r3

virt_entry_2:
	cps #31
	ldr sp, =initial_stack_cpu2_end
	ldr sp, [sp]
	sub sp, #8

123:
	b 123b

	ldr r3, =SetupSecondary
	bx r3
2:
	b 2b

.endfunc
.size secondary_entry, .-secondary_entry


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

.endif

.if M3==1
.syntax unified

.global M3_Reset,M3_NMI,M3_HardFault,M3_MemManage,M3_BusFault,M3_UsageFault,M3_SVCall,M3_DebugMonitor,M3_PendSV,M3_SysTick,M3_ExtIntN

//0
.word 0
//1
/*.word 9
	mov r0, 0
	movt r0, 0x4802
	mov r1, 'a'
	str r1, [r0]
loop:
	b loop

*/
.word M3_Reset+1
//2
.word M3_NMI+1
//3
.word M3_HardFault+1
//4
.word M3_MemManage+1
//5
/*.word M3_BusFault+1*/
.word BusFaultTramp+1
//6
/*.word M3_UsageFault+1*/
.word UsageFaultTramp+1
//7-10, reserved
.word 0
.word 0
.word 0
.word 0
//11
.word M3_SVCall+1
//12
.word M3_DebugMonitor+1
//13, reserved
.word 0
//14
.word M3_PendSV+1
//15
.word M3_SysTick+1
//16+N
.word M3_ExtIntN+1
.fill 495, 4, 0xffffffff

UsageFaultTramp:
	mov r0, sp
	b M3_UsageFault
BusFaultTramp:
	mov r0, sp
	b M3_BusFault

.endif
