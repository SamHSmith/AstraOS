

.global atomic_s64_set
atomic_s64_set:
    lr.d t0, (a0)
    sc.d t1, a1, (a0)
    bnez t1, atomic_s64_set
    mv a0, t0
    ret

.global atomic_s64_increment
atomic_s64_increment:
    lr.d t0, (a0)
    mv t1, t0
    addi t2, t0, 1
    sc.d t0, t2, (a0)
    bnez t0, atomic_s64_increment
    mv a0, t1
    ret

.global atomic_s64_decrement
atomic_s64_decrement:
    lr.d t0, (a0)
    mv t1, t0
    addi t2, t0, -1
    sc.d t0, t2, (a0)
    bnez t0, atomic_s64_decrement
    mv a0, t1
    ret

.global atomic_s64_read
atomic_s64_read:
    lr.d a0, (a0)
    ret

.global atomic_s64_add
atomic_s64_add:
    lr.d t0, (a0)
    mv t1, t0
    add t2, t0, a1
    sc.d t0, t2, (a0)
    bnez t0, atomic_s64_add
    mv a0, t1
    ret








.global atomic_s64_increment_no_stack
atomic_s64_increment_no_stack:
    lr.d t0, (a0)
    addi t2, t0, 1
    sc.d t1, t2, (a0)
    bnez t1, atomic_s64_increment_no_stack
    mv a0, t0
    jalr ra
 
.global atomic_s64_decrement_no_stack
atomic_s64_decrement_no_stack:
    lr.d t0, (a0)
    addi t2, t0, -1
    sc.d t1, t2, (a0)
    bnez t1, atomic_s64_decrement_no_stack
    mv a0, t0
    jalr ra
