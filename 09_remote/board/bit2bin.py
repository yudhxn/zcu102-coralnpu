#!/usr/bin/env python3
"""Vivado .bit -> fpga_manager용 .bin 변환 (bootgen 불필요)

.bit = 헤더(디자인명/파트/날짜) + 실데이터. fpga_manager(ZynqMP)는 헤더 없는
.bin을 요구한다. 표준 파서로 헤더를 벗겨낸다. 만약 로드가 실패하면
*_swapped.bin (바이트 스왑본)으로 재시도할 것.

사용: python3 bit2bin.py coral_base.bit
"""
import struct, sys, os

fn = sys.argv[1]
d = open(fn, "rb").read()
p = 0
l, = struct.unpack(">H", d[p:p+2]); p += 2 + l      # 초기 헤더
l, = struct.unpack(">H", d[p:p+2]); p += 2          # 'a' 키 직전 길이
data = None
while p < len(d):
    key = d[p:p+1]; p += 1
    if key == b"e":
        n, = struct.unpack(">I", d[p:p+4]); p += 4
        data = d[p:p+n]
        break
    l, = struct.unpack(">H", d[p:p+2]); p += 2
    val = d[p:p+l]; p += l
    print("  %s: %s" % (key.decode(), val.rstrip(b"\x00").decode(errors="replace")))
if data is None:
    sys.exit("'e' 레코드를 못 찾음 — .bit 파일이 맞는지 확인")

base = os.path.splitext(fn)[0]
open(base + ".bin", "wb").write(data)
sw = bytearray(len(data))
sw[0::4], sw[1::4], sw[2::4], sw[3::4] = data[3::4], data[2::4], data[1::4], data[0::4]
open(base + "_swapped.bin", "wb").write(bytes(sw))
print("저장: %s.bin (%dB), %s_swapped.bin" % (base, len(data), base))
