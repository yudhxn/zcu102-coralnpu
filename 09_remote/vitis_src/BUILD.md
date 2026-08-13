# 28×28 MNIST 베어메탈 빌드 (Vitis) — 2026-08-12

리눅스 트랙이 버전 호환 문제로 막혀서, **7/28에 이미 성공한 베어메탈 경로**로 전환.
모델·커널·정확도는 어젯밤 에뮬레이터로 검증 완료됐고, 여기서는 **빌드만** 하면 된다.

## 이 폴더의 파일 (전부 `coral_app/src/`에 넣을 것)

| 파일 | 내용 |
|---|---|
| `main_mnist28.c` | A53 로더 — NPU에 프로그램·가중치 적재, 10장 추론, 아스키 아트 출력 |
| `prog28.h` | NPU 커널 기계어 (rv32im, 1,328B) |
| `weights28.h` | int8 가중치 (바이트 패킹, 25.4KB) |
| `demo28.h` | 데모 샘플 10장 (0~9) |
| `expected28.h` | 에뮬레이터 기대 정확도 (9,641 / 10,000) |

## 절차

### 1. Vitis 실행 → 기존 워크스페이스 열기

```
C:\Users\ehdgn\SOTA\zcutonpu\01_hw\vitis_ws
```

`coral_platform`(XSA 기반)과 `coral_app`이 복구돼 있다.

### 2. 앱 소스 교체

`coral_app/src/` 안의 **기존 `main_*.c`를 전부 삭제하거나 Exclude from Build**
(`main`이 둘이면 링크 에러). 그 다음 이 폴더의 **5개 파일을 복사해 넣는다.**

탐색기로 드래그하거나, Vitis에서 `src` 우클릭 → Import → File System.

### 3. Build

`coral_app` 우클릭 → **Build Project** → `coral_app.elf` 생성

**예상 크기 증가**: 가중치가 25KB라 ELF가 8×8 때보다 커진다 (정상).

### 4. BOOT.bin 생성

`Vitis` 메뉴 → **Create Boot Image** (Zynq UltraScale+)

| 순서 | 파일 | 타입 |
|---|---|---|
| 1 | `fsbl.elf` | **bootloader** |
| 2 | `coralnpu_wrapper.bit` | datafile |
| 3 | `coral_app.elf` | datafile |

PMU 펌웨어는 넣지 않는다 (7/27에 없어도 정상 동작 확인).

### 5. SD 부팅

SD 루트에 `BOOT.bin` 복사. **`image.ub`는 삭제**(리눅스 잔재).
SW6 = 1-ON / 2·3·4-OFF → 전원 ON → PuTTY COM3 115200.

## 기대 출력

```
===== MNIST 28x28 on Coral NPU (FC 784-32-10 int8) =====
weights byte-packed 25.4KB / DTCM 32KB   int8 acc 9641/10000 (emulator)

   (28x28 손글씨 아스키 아트)
   NPU -> 0   (label 0)  OK
   ...
===== 10 / 10 correct =====
=== done ===
```

## 주의

- 28×28 아스키 아트는 **가로 59칸**을 쓴다. PuTTY 창을 80칸 이상으로 (기본값이면 OK)
- 가중치 적재에 시간이 조금 걸린다 (25KB를 워드 단위로 6,272번 write)
- `TIMEOUT`이 뜨면 NPU가 안 돈 것 → 비트스트림이 BOOT.bin에 들어갔는지 확인

## 배경 (왜 바이트 패킹인가)

| 방식 | 가중치 25,088개 | DTCM 32KB |
|---|---|---|
| int8 하나를 int32 워드에 (8×8 방식) | 100KB | **초과 — 불가** |
| int8 4개를 워드 하나에 (바이트 패킹) | 25.4KB | 수납 가능 |

NPU 커널이 `lw`로 워드를 읽어 시프트·마스크로 4개를 꺼낸다.
`lb`(바이트 로드)를 안 쓴 이유는 TCM의 바이트 레인 동작에 의존하지 않기 위해서다.
