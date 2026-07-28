.section .text.init
.globl _start
_start:
    lui  sp, %hi(0x17000)
    addi sp, sp, %lo(0x17000)
    call s2_main
1:  j 1b
