TEXT _RFORK(SB), 1, $0
MOVW R3, 0(FP)
MOVW $19, R3
SYSCALL
RETURN
