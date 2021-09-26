
.global read_fp_register
read_fp_register:
    mv x10, fp
    addi x10, x10, -16
    ld x10, (x10)
    ret

.global read_pc_register
read_pc_register:
    mv x10, ra
    addi x10, x10, -4
    ret

.global read_ra_register
read_ra_register:
    mv x10, fp
    addi x10, x10, -8
    ld x10, (x10)
    ret
