TEXT _EXITS(SB), 1, $0
MOVW R3, 0(FP)
MOVW $8, R3
SYSCALL
RETURN