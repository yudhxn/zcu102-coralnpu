#!/usr/bin/env python3
"""mnist28.bin을 unicorn(rv32)으로 실행해 numpy 정수 시뮬과 대조"""
import numpy as np, struct, json, sys
from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32
from unicorn.riscv_const import UC_RISCV_REG_PC

N = int(sys.argv[1]) if len(sys.argv) > 1 else 60

# 가중치/기대값 로드 (numpy 정수 시뮬 재현)
import re
h = open("../sw/weights28.h").read()
def arr(name, dt):
    m = re.search(name + r"\[\d+\] = \{(.*?)\};", h, re.S)
    return np.array([int(x) for x in m.group(1).replace("\n"," ").split(",") if x.strip()], dtype=dt)
W1 = arr("qW1_28", np.int64).reshape(32, 784)
b1 = arr("qb1_28", np.int64)
W2 = arr("qW2_28", np.int64).reshape(10, 32)
b2 = arr("qb2_28", np.int64)
M1 = int(re.search(r"M1_28 (\d+)", h).group(1))
X = np.fromfile("../sw/t10k_x.bin", np.uint8).reshape(-1, 784)
Y = np.fromfile("../sw/t10k_y.bin", np.uint8)

def np_infer(x):
    a = W1 @ x.astype(np.int64) + b1
    a = np.maximum(a, 0)
    hq = np.clip((a * M1) >> 20, 0, 127)
    z = W2 @ hq + b2
    return int(z.argmax()), z

prog = open("mnist28.bin", "rb").read()
DT = 0x10000
mu = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
mu.mem_map(0x0, 0x8000)
mu.mem_map(DT, 0x8000)
mu.mem_write(0, prog)
# 가중치 적재 (한 번)
mu.mem_write(DT + 0x0320, W1.astype(np.int8).tobytes())
mu.mem_write(DT + 0x6520, b1.astype(np.int32).tobytes())
mu.mem_write(DT + 0x65A0, W2.astype(np.int8).tobytes())
mu.mem_write(DT + 0x66E0, b2.astype(np.int32).tobytes())

ok = logit_ok = 0
for t in range(N):
    mu.mem_write(DT + 0x0000, X[t].tobytes())
    mu.mem_write(DT + 0x6734, b"\0\0\0\0")          # DONE=0
    mu.reg_write(UC_RISCV_REG_PC, 0)
    mu.emu_start(0, -1, timeout=10_000_000, count=3_000_000)
    done = struct.unpack("<I", mu.mem_read(DT + 0x6734, 4))[0]
    pred = struct.unpack("<i", mu.mem_read(DT + 0x6730, 4))[0]
    out  = np.frombuffer(mu.mem_read(DT + 0x6708, 40), np.int32)
    np_p, np_z = np_infer(X[t])
    if not done: print(f"[{t}] DONE 미설정!"); break
    if pred == np_p: ok += 1
    if np.array_equal(out.astype(np.int64), np_z): logit_ok += 1
    else: print(f"[{t}] 로짓 불일치 emu={out} np={np_z}")
print(f"\n{N}개: 예측 일치 {ok}/{N}, 로짓(10개 값) 완전 일치 {logit_ok}/{N}")
print("검증", "통과 — RISC-V 코드가 numpy 정수 시뮬과 비트 단위 동일" if logit_ok == N else "실패")
