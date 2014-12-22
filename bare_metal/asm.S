.global assert_func

.func assert_func
assert_func:
1:
	b 1b
	bx lr
.endfunc


.if M3==0

.global InvokeSyscall
.global ChangeModeAndJump

.global EnableIrq
.global EnableFiq
.global EnableIrqFiqForMode
.global IsIrqEnabled
.global GetCpuId
.global ReadDFAR
.global ReadDFSR
.global ReadTPIDRURO
.global WriteTPIDRURO

.global SpinLockInternal
.global SpinUnlockInternal
.global SpinInitInternal

.global SpinLockRecInternal
.global SpinUnlockRecInternal
.global SpinInitRecInternal

.func ReadDFAR
ReadDFAR:
	mrc p15, 0, r0, c6, c0, 0
	bx lr
.endfunc

.func ReadDFSR
ReadDFSR:
	mrc p15, 0, r0, c5, c0, 0
	bx lr
.endfunc

.func ReadTPIDRURO
ReadTPIDRURO:
	mrc p15, 0, r0, c13, c0, 3
	bx lr
.endfunc

.func WriteTPIDRURO
WriteTPIDRURO:
	mcr p15, 0, r0, c13, c0, 3
	bx lr
.endfunc

.func InvokeSyscall
InvokeSyscall:
	push {r7}
	mov r7, r0
	mov r0, r1
	mov r1, r2
	mov r2, r3
	swi 0
	pop {r7}
	bx lr
.endfunc
.size InvokeSyscall, .-InvokeSyscall

/////////////////////////////////

.func SpinInitInternal
SpinInitInternal:
	dmb				//necessary?

	mov r1, #0xff
	str r1, [r0]

	dsb				//necessary?
	bx lr
.endfunc
.size SpinInitInternal, .-SpinInitInternal

.func SpinUnlockInternal
SpinUnlockInternal:
	ldr r1, [r0]			//check it is locked
	cmp r1, #0xff
	bne 1f

	dmb

	mov r1, #0x0
	str r1, [r0]

	dsb
	sev

	bx lr
1:
	bkpt
.endfunc
.size SpinUnlockInternal, .-SpinUnlockInternal

//http://stackoverflow.com/questions/16329123/use-of-strexeq-instead-of-strex-for-spinlock-implementation-in-arm

//http://infocenter.arm.com/help/topic/com.arm.doc.ddi0344k/ch08s05s03.html
//but the args for strex are in the wrong order
.func SpinLockInternal
SpinLockInternal:
	mov r1, #0xff
1:
	ldrex r2, [r0]
	cmp r2, #0				//0 = unlocked

	wfene					//sleep on locked

	strexeq r2, r1, [r0]
	cmpeq r2, #0			//0 = success from strex
	bne 1b

	dmb
	bx lr
.endfunc
.size SpinLockInternal, .-SpinLockInternal

///////////////

//r0 = lock addr
.func SpinInitRecInternal
SpinInitRecInternal:
	ands r2, r0, #3			//check 8b alignment
	bne 1f

	dmb

	mov r1, #0
	str r1, [r0]
	str r1, [r0, #4]

	dsb

	bx lr

1:
	bkpt
.endfunc
.size SpinInitRecInternal, .-SpinInitRecInternal

//r0 = lock addr
//r1 = thread id
.func SpinUnlockRecInternal
SpinUnlockRecInternal:
	ldrd r2, r3, [r0]
	cmp r2, r1				//check it is locked by this thread
	bne 1f

	cmp r3, #0				//check the lock count
	beq 1f

	cmp r3, #1

	moveq r2, #0			//unlock the whole thing
	moveq r3, #0

	subne r3, #1			//dec ref count

	dmb
	strd r2, r3, [r0]
	dsb
	sev

	bx lr

1:
	bkpt
.endfunc
.size SpinUnlockRecInternal, .-SpinUnlockRecInternal

//r0 = lock addr
//r1 = thread id
.func SpinLockRecInternal
SpinLockRecInternal:
	push {r4}

	/*
	r0 lock addr
	r1 curr thread id
	r2 in-lock thread id
	r3 in-lock count
	r4 ex status
	*/
1:
	ldrexd r2, r3, [r0]

	cmp r3, #0				//0 = unlocked
	beq 2f

	cmp r2, r1				//already locked by this thread?
	beq 3f

	wfe					//sleep
	b 1b					//keep trying

2:	//new lock, write thread id and one
	mov r2, r1
	mov r3, #1

	strexd r4, r2, r3, [r0]
	cmp r4, #0			//0 = success from strex
	bne 1b

	dmb

	pop {r4}
	bx lr

3:	//update existing lock
	add r3, #1

	strexd r4, r2, r3, [r0]
	cmp r4, #0			//0 = success from strex
	bne 1b

	dmb

	pop {r4}
	bx lr

.endfunc
.size SpinLockRecInternal, .-SpinLockRecInternal


.func ChangeModeAndJump
ChangeModeAndJump:
	ldr sp, [r3, #8]
	rfeia r3
.endfunc
.size ChangeModeAndJump, .-ChangeModeAndJump

.global EnableFpu
.func EnableFpu
EnableFpu:
	/*
	#unreadable in qemu, unwritable in non-secure...why bother
	#get existing nsacr
	mrc p15, 0, r1, c1, c1, 2
	orr r1, #(3 << 10)
	#set it
	mcr p15, 0, r1, c1, c1, 2*/

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
	mcr p15, 0, r0, c8, c3, 0			//TLBIALLIS

	#ensure flush is finished
	dsb

	#get instruction fetch working ok
	isb

	bx lr
.endfunc
.size FlushTlb, .-FlushTlb

.func IsIrqEnabled
IsIrqEnabled:
	mrs r0, cpsr
	mvn r0, r0
	and r0, #128
	lsr r0, #7
	bx lr

.endfunc
.size IsIrqEnabled, .-IsIrqEnabled

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

.func GetCpuId
GetCpuId:
	mrc p15, 0, r0, c0, c0, 5
	ands r0, #1					/* only support cpu 0 and 1 */
	bx lr
.endfunc
.size GetCpuId, .-GetCpuId

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

.global InsertPageTableLoAndFlush
.func InsertPageTableLoAndFlush
InsertPageTableLoAndFlush:
	mrc p15, 0, r1, c2, c0, 0
	cmp r0, r1
	bxeq lr

	push {lr}
	push {r0}

	bl v7_flush_icache_all
	bl v7_flush_dcache_all

	pop {r0}

	mcr p15, 0, r0, c2, c0, 0

	bl FlushTlb

	pop {pc}
.endfunc
.size InsertPageTableLoAndFlush, .-InsertPageTableLoAndFlush

.global OmapSmc
.func OmapSmc
OmapSmc:
	push {r4-r12, lr}

	#the thing to call
	mov r12, r2

	#r0
	cmp r0, #0
	mov r2, r0
	ldrne r0, [r2]
	#r1
	cmp r1, #0
	mov r3, r1
	ldrne r1, [r3]

	#call it
	push {r2, r3}
	smc 1
	pop {r2, r3}

	#r0
	cmp r2, #0
	strne r0, [r2]
	#r1
	cmp r3, #0
	strne r1, [r3]

	pop {r4-r12, pc}
.endfunc
.size OmapSmc, .-OmapSmc

.global DelaySecond
.func DelaySecond
DelaySecond:
	ldr r0, =555555555
1:
	subs r0, #1
	bne 1b
	bx lr
.endfunc
.size DelaySecond, .-DelaySecond

.global DelayMillisecond
.func DelayMillisecond
DelayMillisecond:
	ldr r0, =555555
1:
	subs r0, #1
	bne 1b
	bx lr
.endfunc
.size DelayMillisecond, .-DelayMillisecond

.global SemiHosting
.func SemiHosting
SemiHosting:
	svc 0x123456
	bx lr
.endfunc
.size SemiHosting, .-SemiHosting


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


.endif

.if M3==1

.syntax unified

.global SemiHosting
.func SemiHosting
SemiHosting:
	bkpt 0xab
	bx lr
.endfunc
.size SemiHosting, .-SemiHosting

.global DelaySecond
.func DelaySecond
DelaySecond:
	ldr r0, =555555555
1:
	subs r0, #1
	bne 1b
	bx lr
.endfunc
.size DelaySecond, .-DelaySecond

.global DelayMillisecond
.func DelayMillisecond
DelayMillisecond:
	ldr r0, =5555
1:
	subs r0, #1
	bne 1b
	bx lr
.endfunc
.size DelayMillisecond, .-DelayMillisecond

.endif

.global TimingTest
TimingTest:
	mov r1, r0
	mov r0, #1024
	lsl r0, #16
1:
	ldr r1, [r1]
	ldr r1, [r1]
	ldr r1, [r1]
	ldr r1, [r1]
	ldr r1, [r1]

	ldr r1, [r1]
	ldr r1, [r1]
	ldr r1, [r1]
	ldr r1, [r1]
	ldr r1, [r1]
	subs r0, #1
	bne 1b
	bx lr

.global TimingTestIssue
TimingTestIssue:
	mov r0, #0
	ldr r1, =0xe000e010
	ldr r2, =0x00010005

	mov r0, #5
	str r0, [r1]
	ldr r3, [r1]

	mov r0, #0
1:
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1
	add r0, #1


	ldr r3, [r1]

	cmp r3, r2
	bne 1b
	//mov r0, r3
	bx lr

.global FuncTest
FuncTest:
	ldr r0, =0x01020003
	bx r0
