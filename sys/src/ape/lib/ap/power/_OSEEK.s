TEXT _OSEEK(SB), 1, $0
MOVW R3, 0(FP)
MOVW $16, R3
SYSCALL
RETURN