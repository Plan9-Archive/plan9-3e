TEXT _FSTAT(SB), 1, $0
MOVW R1, 0(FP)
MOVW $11, R1
SYSCALL
RET
