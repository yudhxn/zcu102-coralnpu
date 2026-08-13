.section .text.init
.globl _start
_start:
    lui  sp, %hi(0x17F00)      /* 스택: DTCM 최상단 (데이터 0x16738 위) */
    addi sp, sp, %lo(0x17F00)
    call main28
1:  j 1b
