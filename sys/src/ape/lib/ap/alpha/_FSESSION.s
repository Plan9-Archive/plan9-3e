TEXT _FSESSION(SB), 1, $0
MOVL R0, 0(FP)
MOVQ $9, R0
CALL_PAL $0x83
RET