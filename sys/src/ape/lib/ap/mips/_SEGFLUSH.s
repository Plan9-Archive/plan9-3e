TEXT _SEGFLUSH(SB), 1, $0
MOVW R1, 0(FP)
MOVW $33, R1
SYSCALL
RET