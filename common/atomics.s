

.global atomic_s64_increment
atomic_s64_increment:
    lr.d t0, (a0)
    addi t0, t0, 1
    sc.d t1, t0, (a0)
    bnez t1, atomic_s64_increment
    mv a0, t0
    ret

.global atomic_s64_decrement
atomic_s64_decrement:
    lr.d t0, (a0)
    addi t0, t0, -1
    sc.d t1, t0, (a0)
    bnez t1, atomic_s64_increment
    mv a0, t0
    ret










.global atomic_s64_increment_no_stack
atomic_s64_increment_no_stack:
    lr.d t0, (a0)
    addi t0, t0, 1
    sc.d t1, t0, (a0)
    bnez t1, atomic_s64_increment_no_stack
    mv a0, t0
    jalr ra
 
.global atomic_s64_decrement_no_stack
atomic_s64_decrement_no_stack:
    lr.d t0, (a0)
    addi t0, t0, -1
    sc.d t1, t0, (a0)
    bnez t1, atomic_s64_increment_no_stack
    mv a0, t0
    jalr ra
