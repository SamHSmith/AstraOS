# Disable generation of compressed instructions.
.option norvc

# Define a .data section.
.section .data

.global KERNEL_START_OTHER_HARTS
KERNEL_START_OTHER_HARTS: .dword 4

# Define a .text.init section.
.section .text.init

# Execution starts here.
.global _start
_start:
    la t1, KERNEL_HART_COUNT
    sd x0, (t1)

	csrr	t0, mhartid
	bnez	t0, 3f

    la t1, KERNEL_START_OTHER_HARTS
    addi t0, x0, 4
    sd t0, (t1)

	# SATP should be zero, but let's make sure
	csrw	satp, zero
	
	# Disable linker instruction relaxation for the `la` instruction below.
	# This disallows the assembler from assuming that `gp` is already initialized.
	# This causes the value stored in `gp` to be calculated from `pc`.
.option push
.option norelax
	la		gp, _global_pointer
.option relax
.option pop
	# Set all bytes in the BSS section to zero.
	la 		a0, _bss_start
	la		a1, _bss_end
	bgeu	a0, a1, 2f
1:
	sd		zero, (a0)
	addi	a0, a0, 8
	bltu	a0, a1, 1b
2:
	# Control registers, set the stack, mstatus, mepc,
	# and mtvec to return to the main function.
	# li		t5, 0xffff;
	# csrw	medeleg, t5
	# csrw	mideleg, t5
	la		sp, _stack_end
	# Setting `mstatus` register:
	# 0b11 << 11: Machine's previous protection mode is 3 (MPP=3).
    # place 13 is for enabling floating point registers
	li		t0, 0b11 << 11 | (1 << 13)
	csrw	mstatus, t0
	# Machine's exception program counter (MEPC) is set to `kinit`.
	la		t1, kinit
	csrw	mepc, t1
    # Machine's trap vector base address is set to `asm_trap_vector`.
    la              t2, m_trap_vector
    csrw    mtvec, t2
	# Set the return address to get us into supervisor mode
	la		ra, kpost_init
	# We use mret here so that the mstatus register is properly updated.
	mret
kpost_init:
	# We set the return address (ra above) to this label. When kinit() is finished
    # it will return here.

    # We need to set the value of mscratch to contain the address of our harts
    # trap stack.
    la t6, KERNEL_TRAP_STACK_TOP
    csrr t5, mhartid
    add t6, t6, t5 #1 # We are indexing the array
    add t6, t6, t5 #2
    add t6, t6, t5 #3
    add t6, t6, t5 #4
    add t6, t6, t5 #5
    add t6, t6, t5 #6
    add t6, t6, t5 #7
    add t6, t6, t5 #8

    ld t6, (t6)
    csrw mscratch, t6

    # We need to set the value of mscratch to contain the address of our harts
    # normal stack.
    la t6, KERNEL_STACK_TOP
    csrr t5, mhartid
    add t6, t6, t5 #1 # We are indexing the array
    add t6, t6, t5 #2
    add t6, t6, t5 #3
    add t6, t6, t5 #4
    add t6, t6, t5 #5
    add t6, t6, t5 #6
    add t6, t6, t5 #7
    add t6, t6, t5 #8
 
    ld t6, (t6)
    mv sp, t6

    # Setting `sstatus` (supervisor status) register:
    # 1 << 8    : Supervisor's previous protection mode is 1 (SPP=1 [Supervisor]).
    # 1 << 5    : Supervisor's previous interrupt-enable bit is 1 (SPIE=1 [Enabled]).
    # 1 << 1    : Supervisor's interrupt-enable bit will be set to 1 after sret.
    # We set the "previous" bits because the sret will write the current bits
    # with the previous bits.
    li        t0, (1 << 8) | (1 << 5) | (1 << 13)
    csrw    sstatus, t0
    la        t1, kmain
    csrw    sepc, t1
 
    # Setting `mie` (machine interrupt enable) register:
    li t2, (1 << 11) | (1 << 7) 
    csrw mie, t2

	# kinit() is required to return back the SATP value (including MODE) via a0
	csrw	satp, a0
	# Force the CPU to take our SATP register.
	# To be efficient, if the address space identifier (ASID) portion of SATP is already
	# in cache, it will just grab whatever's in cache. However, that means if we've updated
	# it in memory, it will be the old table. So, sfence.vma will ensure that the MMU always
	# grabs a fresh copy of the SATP register and associated tables.
	sfence.vma
	# sret will put us in supervisor mode and re-enable interrupts
    sret
3:

    # Parked harts go here.

    # SATP should be zero, but let's make sure
    csrw    satp, zero

    # Setting `mstatus` register:
    # 0b11 << 11: Machine's previous protection mode is 3 (MPP=3).
    # place 13 is for enabling floating point registers
    li        t0, 0b11 << 11 | (1 << 13)
    csrw    mstatus, t0
    # Machine's exception program counter (MEPC) is set to `kinit`.
    la        t1, continue
    csrw    mepc, t1

    # Machine's trap vector base address is set to `asm_trap_vector`.
    la              t2, m_trap_vector
    csrw    mtvec, t2
    mret

continue:

    mv t6, x0
.rept 20
    addi t6, t6, 2000
.endr
loop:
    addi t6, t6, -1
    bnez t6, loop
    

    la t1, KERNEL_START_OTHER_HARTS
    ld t1, (t1)
    bnez t1, continue

    la a0, KERNEL_HART_COUNT
    la ra, ready
    j atomic_s64_increment_no_stack
ready:
    la t1, KERNEL_START_OTHER_HARTS
    ld t1, (t1)
    beq t1, x0, ready

enter_kernel:

    # We need to set the value of mscratch to contain the address of our harts
    # trap stack.
    la t6, KERNEL_TRAP_STACK_TOP
    csrr t5, mhartid
    add t6, t6, t5 #1 # We are indexing the array
    add t6, t6, t5 #2
    add t6, t6, t5 #3
    add t6, t6, t5 #4
    add t6, t6, t5 #5
    add t6, t6, t5 #6
    add t6, t6, t5 #7
    add t6, t6, t5 #8

    ld t6, (t6)
    csrw mscratch, t6

    # We need to set the value of mscratch to contain the address of our harts
    # normal stack.
    la t6, KERNEL_STACK_TOP
    csrr t5, mhartid
    add t6, t6, t5 #1 # We are indexing the array
    add t6, t6, t5 #2
    add t6, t6, t5 #3
    add t6, t6, t5 #4
    add t6, t6, t5 #5
    add t6, t6, t5 #6
    add t6, t6, t5 #7
    add t6, t6, t5 #8

    ld t6, (t6)
    mv sp, t6
 
    # Setting `sstatus` (supervisor status) register:
    # 1 << 8    : Supervisor's previous protection mode is 1 (SPP=1 [Supervisor]).
    # 1 << 5    : Supervisor's previous interrupt-enable bit is 1 (SPIE=1 [Enabled]).
    # 1 << 1    : Supervisor's interrupt-enable bit will be set to 1 after sret.
    # We set the "previous" bits because the sret will write the current bits
    # with the previous bits.
    li        t0, (1 << 8) | (1 << 5) | (1 << 13)
    csrw    sstatus, t0
    la        t1, kmain_hart
    csrw    sepc, t1
 
    # Setting `mie` (machine interrupt enable) register:
    li t2, (1 << 11) | (1 << 7) 
    csrw mie, t2

    # set the correct satp for the kernel
    la t6, KERNEL_SATP_VALUE
    ld t6, (t6)
    csrw satp, t6

    sfence.vma
    csrr a0, mhartid
    sret

has_entered:
    nop
    j has_entered

4:
    wfi
    j        4b

.global kernel_rdtime
kernel_rdtime:
    rdtime a0
    ret

.global kernel_rdcycle
kernel_rdcycle:
    rdcycle a0
    ret

.global kernel_rdinstret
kernel_rdinstret:
    rdinstret a0
    ret
