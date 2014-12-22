.global assert_func

.func assert_func
assert_func:
1:
	b 1b
	blr
.endfunc

.global TimingTest
.func TimingTest
.if 1
TimingTest:
	li r2, 1024
	slwi r2, r2, 16
1:
	lwz r3, 0(r3)
	lwz r3, 0(r3)
	lwz r3, 0(r3)
	lwz r3, 0(r3)
	lwz r3, 0(r3)

	lwz r3, 0(r3)
	lwz r3, 0(r3)
	lwz r3, 0(r3)
	lwz r3, 0(r3)
	lwz r3, 0(r3)

	subi r2, r2, 1
	cmpwi r2, 0
	bne 1b
	blr
.else
TimingTest:
	li r2, 512
	li r3, 0
	slwi r2, r2, 16
1:
	addi r3, r3, 1
	addi r3, r3, 1
	addi r3, r3, 1
	addi r3, r3, 1
	addi r3, r3, 1

	addi r3, r3, 1
	addi r3, r3, 1
	addi r3, r3, 1
	addi r3, r3, 1
	addi r3, r3, 1

	subi r2, r2, 1
	cmpwi r2, 0
	bne 1b
	blr
.endif
.endfunc
