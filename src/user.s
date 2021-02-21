

.global user_surface_commit
user_surface_commit:
    mv a1, a0
    addi a0, x0, 0
    ecall
    ret

.global user_surface_acquire
user_surface_acquire:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 1
    ecall
    ret
