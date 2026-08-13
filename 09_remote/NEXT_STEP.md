# 다음 단계 (2026-08-14 아침)

## 어제(08-13) 도달한 지점

| 항목 | 상태 |
|---|---|
| PetaLinux 2026.1 리눅스 부팅 | ✅ (지난주 3전 3패였던 것) |
| SSH 원격 접속 | ✅ |
| PL 비트스트림 | ✅ `fpga0/state = operating` |
| PL 클럭 | ✅ 50MHz, `clk_ignore_unused` |
| PL 전원 도메인 | ✅ `pd_ignore_unused` |
| **NPU 주소 읽기** | ✅ **ITCM/DTCM 정상 응답** |
| NPU 주소 쓰기 | ❌ SError → 커널 패닉 |

### 결정적 증거 — 주소 경계가 사양과 일치

```
0x5_0000_0000 + 0x0000   READ OK      ITCM 시작
0x5_0000_0000 + 0x2000   BUS ERROR    ITCM 8KB 끝    ← 사양 일치
0x5_0000_0000 + 0x10000  READ OK      DTCM 시작
0x5_0000_0000 + 0x18000  BUS ERROR    DTCM 32KB 끝   ← 사양 일치
0x5_0000_0000 + 0x30000  BUS ERROR    CSR (읽기 미지원 추정)
```

경계가 8KB/32KB에서 정확히 떨어진다 = PL에 올라간 것이 우리 Coral 코어이고
리눅스에서 정상 응답 중이라는 뜻. **하드웨어와 통로는 문제가 없다.**

## 남은 문제 — 쓰기만 실패

ARM에서 장치 메모리 쓰기 실패는 **비동기(SError)** 로 통보돼 커널 패닉이 된다.
읽기는 동기라 SIGBUS로 잡히고 보드가 산다. 그래서 `rdprobe` 는 살아남았다.

### 가설 (우선순위 순)

1. **CSR 접근이 원인** — `coral_probe` 는 쓰기 후 `0x30008`(CSR status)을 읽는다.
   CSR은 읽기를 지원하지 않는 것으로 보이므로 여기서 터졌을 가능성.
   → 베어메탈 코드도 CSR에는 쓰기만 했다. 읽기를 빼면 해결될 수 있다.

2. **좁은 폭 쓰기 미지원** — 128bit 슬레이브에 32bit 쓰기(wstrb 부분 세트).
   베어메탈에서는 됐으므로 가능성은 낮다.

3. **비보안(EL0) 쓰기 차단** — 베어메탈은 EL3(보안)에서 실행됐다.
   읽기가 되는데 쓰기만 막히는 설정은 드물어 가능성 낮음.

### 내일 첫 실험

가설 1이 맞는지 보는 최소 테스트:
**DTCM에 32bit 한 워드만 쓰고, 읽기는 DTCM에서만 하는 프로그램.**
CSR을 건드리지 않는다. 이게 살아남으면 가설 1 확정.

Claude에게 "쓰기 테스트 바이너리 만들어줘" 라고 하면 만들어준다.
(`09_remote/sw/rdprobe.c` 와 같은 방식, zig cc 로 정적 aarch64 빌드)

## 부팅 절차 (매번 필요 — 아직 영구 반영 전)

u-boot 프롬프트(`ZynqMP>`)에서:
```
setenv bootargs "console=ttyPS0,115200 clk_ignore_unused pd_ignore_unused rdinit=/bin/sh root=/dev/ram0 rw"
load mmc 0:1 0x10000000 image.ub
bootm 0x10000000
```
`#` 프롬프트에서:
```
mount -t proc proc /proc; mount -t sysfs sys /sys; mount -t devtmpfs dev /dev; mount -t vfat /dev/mmcblk0p1 /mnt
```

**주의**: `boot` 이 아니라 `bootm 0x10000000`. `init=` 이 아니라 `rdinit=`.

## 영구 반영 대기 (쓰기 문제 해결 후 한 번에)

`fix_linux.sh` 에 아래를 넣고 재빌드하면 위 수동 절차가 사라진다.
- `pd_ignore_unused` 추가 (현재 `clk_ignore_unused` 만 들어있음)
- root 비밀번호가 실제로 이미지에 반영되는지 확인 (지난번 image.ub 미갱신)
- 고정 IP 10.126.37.200

## 하지 말 것

`/mnt/board/coral_probe` (인자 없이) — 쓰기를 해서 보드를 죽인다.
`rdprobe` 는 읽기 전용이라 안전.
