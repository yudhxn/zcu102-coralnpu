.section .text.init
.globl _start
_start:
    lui  sp, %hi(0x17000)     /* 스택: 스크래치 영역 상단 */
    addi sp, sp, %lo(0x17000)
    call cnn_main
1:  j 1b
