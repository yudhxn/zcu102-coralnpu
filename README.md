# zcu102-coralnpu

Xilinx **ZCU102** (Zynq UltraScale+ MPSoC, XCZU9EG) 보드에 **Google Coral NPU**를 올려
**input → 추론 → output** 을 실제로 동작시키는 프로젝트.

- 기간: 2026-07-13 ~ 2026-08-31 (교수님 데모)
- 보드: Xilinx ZCU102 **Rev 1.1**
- NPU: **Google Coral NPU** (RISC-V 기반, 오픈소스)
- 상태: ✅ **M0–M6 완료** + 8×8 MNIST/CNN 보드 검증 완료
  · 🚧 확장 트랙 진행 중 — 28×28 MNIST, 리눅스/SSH, GitHub CI 원격 추론 (`09_remote/`)

---

## 왜 Coral NPU인가

**Coral NPU**는 Google Research / DeepMind가 공동 설계해 2025년 10월 공개한 오픈소스 NPU다.
32-bit RISC-V ISA 기반이며 **matrix + vector(SIMD) + scalar** 세 개의 처리 유닛으로 구성된다.

| 항목 | 내용 |
|---|---|
| ISA | `rv32imf_zve32x_zicsr_zifencei_zbb` |
| 파이프라인 | 4-stage, in-order dispatch / out-of-order retire |
| 병렬성 | 4-way scalar, 2-way vector dispatch |
| SIMD | 128-bit (256-bit 예정) |
| 메모리 | ITCM 8KB / DTCM 32KB (single-cycle SRAM) |
| 버스 | **AXI4** manager + subordinate |

AXI4 인터페이스를 manager/subordinate 양쪽으로 제공한다는 점이 중요하다.
ZCU102의 ARM(PS)이 Coral(PL)을 설정하고 구동하는 구조를 만들 수 있기 때문이다.
그리고 고정 기능 가속기가 아니라 **작은 RISC-V 프로세서**이기 때문에, C로 짠 추론
커널을 컴파일해 ITCM에 넣으면 어떤 모델이든 하드웨어 재합성 없이 실행할 수 있다.

---

## 시스템 구성

```
┌─────────────────────── ZCU102 (XCZU9EG) ───────────────────────┐
│                                                                │
│   PS (ARM Cortex-A53)              PL (FPGA)                   │
│   └─ 로더 (베어메탈 or 리눅스)      └─ Coral NPU               │
│        · 커널·W·X 주입                   · scalar (RISC-V)     │
│        · 결과 수신·검증                  · vector (SIMD)       │
│        · UART/SSH 출력                   · matrix (MAC)        │
│              └────────── AXI4 ──────────┘                      │
└────────────────────────────────────────────────────────────────┘
```

PS가 s_axi(`0x5_0000_0000`)로 Coral의 ITCM(+0x0) / DTCM(+0x10000) / CSR(+0x30000)에
접근해 프로그램·데이터를 넣고 실행·결과를 회수한다. 실행 경로는 두 가지:

- **베어메탈** (기본 데모): BOOT.bin(SD)만으로 부팅 → 자동 추론. 컴퓨터·JTAG 불필요.
- **리눅스** (확장, `09_remote/`): 프리빌트 리눅스로 부팅 → `/dev/mem` mmap으로 동일
  주소 접근 → **SSH 원격 실행 + GitHub CI 자동 추론**. 비트스트림은 fpga_manager로
  런타임 로드하므로 **Vivado/Vitis 빌드가 전혀 필요 없다.**

---

## 툴체인 흐름

Coral의 RTL은 Verilog가 아니라 **Chisel(Scala)** 로 작성되어 있어,
Vivado에 넣기 전에 Bazel로 Verilog를 생성하는 단계가 필요하다.

```
[하드웨어 — 1회]
hdl/chisel (Scala) ─Bazel(WSL2)→ Verilog ─Vivado 2026.1→ bitstream ─bootgen→ BOOT.bin

[소프트웨어 — 모델 바뀔 때마다, 하드웨어 재합성 불필요]
train.py(학습) → quant.py(int8 양자화) → 커널.c ─riscv gcc/zig(rv32im)→ ITCM 프로그램
                                                 └─ unicorn 에뮬레이터로 정확도 사전 검증
```

---

## 리포지토리 구조

| 경로 | 내용 |
|---|---|
| `docs/` | 실험 일지(LOG.md), 발표 자료(Coral_NPU_ZCU102.pptx) |
| `01_hw/` | RTL (coral_axi_wrapper.sv, CoreMiniAxi.sv), **rebuild_project.tcl**(프로젝트 자동 재건), REBUILD.md |
| `03_sw/` | 베어메탈 앱(main_mnist.c, main_cnn.c 등) + NPU 커널 소스(npu_src/) |
| `05_notes/` | 작업 노트, 주차 계획 |
| `05_results/` | 성능·리소스 측정(M6_analysis.md), **uart_capture/**(보드 실제 출력 캡처) |
| `06_theory/` | 이론 정리 노트 |
| `09_remote/` | **원격/리눅스 트랙** — 정적 바이너리, 28×28 MNIST, SD 준비 스크립트, CI 가이드 |
| `.github/workflows/` | **board-infer.yml** — push 시 실제 보드에서 NPU 추론 실행·검증 |

---

## 마일스톤

| | 목표 | 상태 |
|---|---|---|
| M0 | 보드 동작 검증 (프리빌트 리눅스 부팅) | ✅ |
| M1 | WSL2 + Bazel 환경 구축, Chisel→Verilog | ✅ |
| M2 | Verilator 시뮬에서 Coral 실행 + 사이클 측정 | ✅ |
| M3 | Coral RTL → ZCU102 합성 (SYNTHESIS define, LUT 18%) | ✅ |
| M4 | Block Design + AXI 연결 + 비트스트림 (50MHz, WNS +0.462) | ✅ |
| M5 | input → 추론 → output 동작 + SD 독립 부팅 | ✅ |
| M6 | 성능·리소스 측정 및 한계 분석 | ✅ |
| E1 | 28×28 실제 MNIST (바이트 패킹으로 DTCM 32KB 수납) | 🚧 에뮬 검증 완료, 보드 검증 대기 |
| E2 | 리눅스 부팅 + SSH + 런타임 비트스트림 로드 | 🚧 준비 완료, 보드 검증 대기 |
| E3 | GitHub CI → 실제 보드 자동 추론 | 🚧 워크플로 완성, 러너 연결 대기 |
| 🎯 | 최종 데모 · 보고 (08-31) | 데모 확보, 발표 준비 |

---

## 결과 요약

### 보드에서 검증 완료

- **PS↔NPU AXI 링크**: A53에서 Coral ITCM/DTCM read/write 일치 확인.
- **NPU 실행**: RISC-V 프로그램을 ITCM에 적재·실행 (scalar `output=input×3`부터 시작).
- **신경망 추론**: Y = ReLU(W·X + b) 완전연결층을 NPU가 계산.
- **MNIST 손글씨 인식** (8×8, 최종 데모): MLP(64-32-10) **int8 양자화** 추론.
  · float32 98.00% → **int8 98.00%** (손실 0), 가중치 9,640B → **2,368B (4배 절감)**
  · 보드에서 0~9 **10/10 정확**, 추론 1회 17,230 명령어 (50MHz 기준 ~345µs)
  · SD 독립 부팅 — 전원만 넣으면 자동 실행. 실제 UART 출력: `05_results/uart_capture/`
- **CNN**: Conv(8ch 3×3)+ReLU+MaxPool+FC도 NPU에서 동작 (int8 96.89%, 10/10).
  · 파라미터 810개(FC의 1/3)·가중치 864B, 연산량은 ~2배 — 메모리/연산 트레이드오프 실측
- **검증 3중 일치**: numpy 정수 시뮬 = unicorn 에뮬레이터 = 실제 보드 출력이
  글자 단위로 동일함을 확인 (2026-08-11 재검증 포함).

### 확장 트랙 (`09_remote/`, 에뮬레이터 검증 완료 · 보드 검증 대기)

- **28×28 실제 MNIST**: FC 784-32-10, float 96.36% → **int8 96.41%** (10,000장).
  가중치 25.4KB를 **바이트 패킹**으로 DTCM 32KB에 수납 (int32 저장 방식이면 100KB로 불가).
  rv32im 커널 1,328B — numpy 정수 시뮬과 **로짓까지 비트 단위 일치**.
- **빌드 없는 배포**: 리눅스용 정적 바이너리(aarch64)를 미리 크로스컴파일 —
  보드에서는 복사·실행만. Vivado/Vitis/라이선스 불필요.
- **원격 운용**: 프리빌트 리눅스 + SSH + fpga_manager 런타임 비트스트림 로드 +
  PL 클럭 런타임 설정(50MHz). `run_demo.sh` 하나로 로드→클럭→탐침→추론 자동화.
- **GitHub CI**: `git push` → self-hosted 러너(PC) → SSH → **실제 보드에서 추론**
  → 10,000장 정확도가 에뮬 기대값(9,641/10,000)과 일치해야 PASS.

---

## 알려진 리스크 / 한계 (요약)

1. **곱셈기 구현 특성** — 기능은 정상(`mul` 실측 확인)이나 DSP48E2 대신 LUT 로직으로
   구현됨(DSP 6개만 사용) → 면적·속도 효율 개선 여지.
2. **타깃 칩 규모 차이** — Coral 기본 타깃 xcvu13p 대비 ZU9EG는 ~6배 작음
   → NPU 코어(CoreMiniAxi)만 분리 합성 (LUT 18%).
3. **클럭 상한 50MHz** — 100MHz 타이밍 위반(WNS -2.385) → 50MHz 하향으로 안정 동작.
4. **ZCU102 공식 미지원** — Coral 저장소는 자사 보드 전제 → AXI wrapper·제약 직접 작성.
5. **TCM 용량** — ITCM 8KB / DTCM 32KB. 대형 모델(예: LeNet-5+런타임 1.7MB)은 수용 불가
   실측 — 경량 구조 탐색(M6)과 바이트 패킹(E1)으로 대응. 상세: `05_results/M6_analysis.md`.

---

## 재해 복구 기록 (2026-08-11)

개발 PC SSD 고장으로 빌드 산출물 전체 유실 → 복구 과정에서 얻은 교훈:

- **산출물(BOOT.bin/XSA/bit)은 저장소 밖에 이중 백업** — `.gitignore`가 바이너리를
  제외하므로 커밋만으로는 보존되지 않는다. (현재 OneDrive `zcu102_backup_20260811/`)
- 프로젝트 자동 재건 스크립트 `01_hw/rebuild_project.tcl` + 절차서 `01_hw/REBUILD.md` 정비.
- 소스·기록(LOG.md)만 있으면 UART 출력까지 비트 단위로 재현 가능함을 확인
  (`05_results/uart_capture/README.md`).

---

## 참고

- Coral NPU: https://github.com/google-coral/coralnpu
- Coral NPU Datasheet: https://developers.google.com/coral/guides/hardware/datasheet
