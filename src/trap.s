# trap.S
# Trap handler and global context
# Steve Operating System
# Stephen Marz
# 24 February 2019


.section .text
.global m_trap_vector
# This must be aligned by 4 since the last two bits
# of the mtvec register do not contribute to the address
# of this vector.
.align 4
m_trap_vector:
	# All registers are volatile here, we need to save them
	# before we do anything.
	# csrrw will atomically swap t6 into mscratch and the old
	# value of mscratch into t6. This is nice because we just
	# switched values and didn't destroy anything -- all atomically!
	# in cpu.rs we have a structure of:
	#  32 gp regs		0
	#  32 fp regs		256
	#  SATP register	512
	#  Trap stack       520
	#  CPU HARTID		528
	# We use t6 as the temporary register because it is the very
	# bottom register (x31)

    addi sp, sp, -65*8

    csrrw t6, mscratch, t6
    mv t6, sp

.altmacro
.macro intreg_cpy num
    sd x\num, (t6)
    addi t6, t6, 8
.endm

.set i,0
.rept 31
    intreg_cpy %i
    .set i,i+1
.endr

    csrrw t5, mscratch, t5
    intreg_cpy 30 # t5/x30 stores the real t6/x31 because t6 is our counter

.macro floatreg_cpy num
    fsd f\num, (t6)
    addi t6, t6, 8
.endm
 
.set i,0
.rept 32
    floatreg_cpy %i
    .set i,i+1
.endr

    csrr a0, satp
    sd a0, (t6)

	# Get ready to go into Rust (trap.rs)
	# We don't want to write into the user's stack or whomever
	# messed with us here.
    csrr	a0, mepc
    csrr	a1, mtval
    csrr	a2, mcause
    csrr	a3, mhartid
    csrr	a4, mstatus
    mv      a5, sp
	call	m_trap

	# When we get here, we've returned from m_trap, restore registers
	# and return.
	# m_trap will return the return address via a0.

    csrw	mepc, a0
    mv t6, sp
 
.altmacro
.macro intreg_place num
    ld x\num, (t6)
    addi t6, t6, 8
.endm
 
.set i,0
.rept 31
    intreg_place %i
    .set i,i+1
.endr
 
    csrrw t5, mscratch, t5
    intreg_place 30 # t5/x30 stores the real t6/x31 because t6 is our counter
    csrrw t5, mscratch, t5 # but this time we swap t6 into mscratch
 
.macro floatreg_place num
    fld f\num, (t6)
    addi t6, t6, 8
.endm
 
.set i,0
.rept 32
    floatreg_place %i
    .set i,i+1
.endr

    ld t6, (t6)
    csrw satp, t6

    # Now swap the real t6 into t6
    csrr	t6, mscratch

    addi sp, sp, 65*8

    sfence.vma

    mret

