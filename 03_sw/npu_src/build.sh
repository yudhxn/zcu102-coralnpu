#!/bin/sh
# NPU(Coral, rv32im)용 C 프로그램 빌드 -> .text 바이너리 추출
# 필요: riscv64-unknown-elf-gcc / -as / -ld / -objcopy  (binutils+gcc)
# 주의: gcc가 시스템 as를 부르는 문제 때문에 -S 로 어셈블리를 뽑아 as로 직접 어셈블한다.
set -e
riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -O2 -ffreestanding -S mlp.c -o mlp.s
riscv64-unknown-elf-as  -march=rv32im -mabi=ilp32 mlp.s -o mlp.o
riscv64-unknown-elf-ld  -m elf32lriscv -T link.ld mlp.o -o mlp.elf
riscv64-unknown-elf-objcopy -O binary --only-section=.text mlp.elf mlp.bin
riscv64-unknown-elf-size mlp.elf
echo "text bytes: $(stat -c%s mlp.bin)"
