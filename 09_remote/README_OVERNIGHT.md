# 밤샘 작업 결과 — 리눅스 + SSH + 원격 추론 + 28×28 MNIST

2026-08-11 밤 준비분. **Vivado/Vitis 빌드 없이** 아래가 가능하도록 만들었다:
보드에 리눅스 부팅 → SSH 접속 → 비트스트림·클럭 런타임 설정 → 실제 NPU에서
8×8(검증본)과 **28×28 MNIST(신규)** 추론 → GitHub CI로 자동화.

## 만들어진 것

```
09_remote/
├── copy_files.ps1        ← PC에서 1회 실행 (비트스트림·리눅스 이미지 복사)
├── sd/                   ← SD에 넣을 것 (BOOT.BIN + image.ub, 2019.1 프리빌트)
├── bit/                  ← coral_base.bit (+ bit2bin 변환 결과)
├── board/                ← ★보드로 가져갈 폴더 (SD FAT 또는 pscp)
│   ├── coral_probe       정적 바이너리: AXI 창 탐침 + PL클럭 50MHz 설정
│   ├── coral_mnist8      기존 8×8 데모(검증본)의 리눅스 포팅 — 첫 테스트용
│   ├── coral_mnist28     ★신규 28×28 MNIST (FC 784-32-10 int8)
│   ├── t10k_x/y.bin      전수 평가용 테스트셋 10,000장
│   ├── bit2bin.py        .bit → fpga_manager용 .bin 변환
│   └── run_demo.sh       전체 실행 스크립트 (비트스트림→클럭→추론)
├── npu/                  RISC-V 커널 소스+빌드 (mnist28: 1,328B, 에뮬 검증 완료)
├── sw/                   학습·양자화·로더 소스 (train28.py 등)
└── ci/RUNNER_SETUP.md    GitHub CI 연결 가이드
```

## 28×28 모델 (완성, 에뮬레이터 검증까지 끝남)

| 항목 | 값 |
|---|---|
| 구조 | FC 784→32(ReLU)→10, 대칭 int8 |
| 정확도 | float32 96.36% → **int8 96.41%** (10,000장, 손실 없음) |
| 가중치 | 25.4KB 바이트 패킹 (기존 int32 방식이면 100KB라 DTCM 초과) |
| 커널 | rv32im 1,328B, lw+시프트 언팩(바이트레인 비의존), 64bit 재양자화 |
| 검증 | unicorn 에뮬 60장: numpy 정수 시뮬과 **로짓까지 비트 단위 일치** |
| CI 기준값 | 9,641 / 10,000 (보드 전수 실행 시 이 값과 일치해야 PASS) |

## 내일(원격) 실행 순서

### 0. 전제 — 퇴근 전 체크리스트가 됐다면
- `copy_files.ps1` 실행됨 (→ `sd/`, `bit/`에 파일 존재)
- SD에 `sd/BOOT.BIN`+`image.ub` 복사, 보드 랜선, 전원 ON, 보드 IP 확인
- PC 원격 데스크톱 가능

### 1. (PC, 원격데스크톱) 비트스트림 변환 — 1회
```
cd C:\Users\ehdgn\SOTA\zcutonpu\zcu102-coralnpu\09_remote\bit
python bit2bin.py coral_base.bit
```
파이썬이 없으면 Vitis 것 사용:
`C:\AMD\2026.1\Vitis\bin\... ` 대신 → 아무 python이나 OK. 그래도 없으면
Cowork 세션에서 나(Claude)에게 "bit 변환해줘"라고 하면 샌드박스에서 변환해준다.

### 2. (PC) 보드로 파일 전송 — 1회
```
pscp -pw root -r C:\Users\ehdgn\SOTA\zcutonpu\zcu102-coralnpu\09_remote\board root@<보드IP>:/run/media/mmcblk0p1/
pscp -pw root C:\...\09_remote\bit\coral_base.bin root@<보드IP>:/run/media/mmcblk0p1/board/
pscp -pw root C:\...\09_remote\bit\coral_base_swapped.bin root@<보드IP>:/run/media/mmcblk0p1/board/
```
(SD FAT 파티션은 재부팅해도 유지됨 = 1회면 충분)

### 3. (SSH) 첫 검증 — 순서 중요
```
ssh root@<보드IP>          # 비번 root
sh /run/media/mmcblk0p1/board/run_demo.sh 8
```
스크립트가 순서대로: 비트스트림 로드 → PL클럭 50MHz → AXI 탐침 → 8×8 추론.
**8×8이 "10 / 10 correct" 나오면 전체 경로(리눅스→AXI→NPU)가 검증된 것.**

### 4. 28×28
```
sh /run/media/mmcblk0p1/board/run_demo.sh 28     # 데모 10장
sh /run/media/mmcblk0p1/board/run_demo.sh full   # 10,000장 전수 (수 분)
```
full이 `9641 / 10000` + "HW/EMU 완전 일치 — PASS"면 성공.

### 5. GitHub CI 연결 (선택, `ci/RUNNER_SETUP.md`)
PC에 러너 설치 + Secrets 2개 → 이후 `git push`마다 실제 보드 추론 자동 실행.

## 문제 발생 시 (예상 시나리오)

| 증상 | 원인/대응 |
|---|---|
| probe: 모든 창 BUS ERROR | 비트스트림 로드 실패 → `run_demo.sh` 출력 중 fpgautil 부분 확인, swapped 재시도 |
| probe: 0x5 창만 실패, 다른 창 PASS | 프리빌트 FSBL의 HPM 설정 차이 → 응답한 주소를 알려주면 로더 재빌드해줌 |
| 8×8 TIMEOUT | 클럭 미설정(100MHz 타이밍 위반) → `coral_probe --clkshow` 값 확인 |
| fpgautil 없음 | run_demo.sh가 sysfs 방식으로 자동 폴백함 |
| SSH 접속 불가 | 보드 IP 변경(DHCP) → PuTTY COM3에서 `ifconfig eth0` 재확인 |
| 전부 막힘 | **기존 SD(MNIST BOOT.bin)로 되돌리면 언제든 베어메탈 데모 가능** — 데모는 안전 |

## 리스크 (솔직한 것)

시뮬레이션으로 검증 못 하는 부분이 두 곳 있다:
1. **프리빌트 FSBL(TRD)의 PS 설정**이 우리 XSA와 달라 0x5 창이 안 열릴 가능성
   → probe가 대체 주소를 찾아줌. 찾으면 알려달라, 로더 수정본 만들어준다.
2. **fpga_manager의 .bin 포맷** (스왑 여부) → 두 버전 다 만들어 자동 재시도.

이 두 개만 통과하면 나머지(커널·양자화·로더 로직)는 이미 비트 단위로 검증돼 있다.

## 추가 발견 (08-11 밤)

- `coral_highmem.xsa` 안의 비트스트림은 헤더를 확인해보니 **base(7/23)와 동일한
  파일**이었다. highmem(ITCM/DTCM 1MB) 비트스트림은 export되지 않았던 것으로 보임.
  → highmem 경로는 당분간 보류. 28×28은 바이트 패킹으로 base에서 해결됨(이미 완료).
- 2019.1 프리빌트 릴리즈에는 `zynqmp_fsbl.elf / pmufw.elf / bl31.elf / u-boot.elf /
  system.dtb`가 개별로 들어있다. 필요 시 bootgen(라이선스 불필요)으로
  "Coral 비트스트림 + 리눅스" BOOT.BIN을 새로 조립하는 백업 경로도 가능.
