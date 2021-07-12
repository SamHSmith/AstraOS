

# a0 holds the memory address
# of the spinlock which is a
# 4 byte word.

.global spinlock_create
spinlock_create:
    lr.w t0, (a0)
    sc.w t0, x0, (a0)
    bnez t0, spinlock_create
    ret

.global spinlock_acquire
spinlock_acquire:
    fence
try_acquire:
    lr.w t0, (a0)
    bnez t0, try_acquire
    addi t1, x0, 1
    sc.w t0, t1, (a0)
    bnez t0, try_acquire
    ret

.global spinlock_try_acquire
spinlock_try_acquire:
    fence
    lr.w t0, (a0)
    bnez t0, acquire_fail
    addi t1, x0, 1
    sc.w t0, t1, (a0)
    bnez t0, acquire_fail
    addi a0, x0, 1
    ret
acquire_fail:
    addi a0, x0, 0
    ret

.global spinlock_release
spinlock_release:
    fence
try_release:
    lr.w t0, (a0)
    sc.w t0, x0, (a0)
    bnez t0, try_release
    ret




.global spinlock_create_no_stack
spinlock_create_no_stack:
    lr.w t0, (a0)
    sc.w t0, x0, (a0)
    bnez t0, spinlock_create_no_stack
    jalr ra
 
.global spinlock_acquire_no_stack
spinlock_acquire_no_stack:
    fence
try_acquire_no_stack:
    lr.w t0, (a0)
    bnez t0, try_acquire_no_stack
    addi t1, x0, 1
    sc.w t0, t1, (a0)
    bnez t0, try_acquire_no_stack
    jalr ra
 
.global spinlock_release_no_stack
spinlock_release_no_stack:
    fence
try_release_no_stack:
    lr.w t0, (a0)
    sc.w t0, x0, (a0)
    bnez t0, try_release_no_stack
    jalr ra
