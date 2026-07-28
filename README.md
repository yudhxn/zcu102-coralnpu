# zcu102-coralnpu

Xilinx **ZCU102** (Zynq UltraScale+ MPSoC, XCZU9EG) 보드에 오픈소스 NPU를 올려
**input → 추론 → output** 을 실제로 동작시키는 프로젝트.

- 기간: 2026-07-13 ~ 2026-08-31 (교수님 데모)
- 보드: Xilinx ZCU102 **Rev 1.1**
- 대상 NPU: **Google Coral NPU** (RISC-V 기반)
- 상태: ✅ **M0–M6 완료** — 신경망 완전연결층 추론 + SD 독립 부팅 동작 확인

> ⚠️ **방향 전환 기록:** 본 프로젝트는 초기에 NVIDIA **NVDLA (nv_small)** 로 시작했으나,
> 2026-07-15 교수님 지시로 **Google Coral NPU** 로 대상을 변경했다.
> NVDLA 단계에서 확보한 보드 부팅 / FSBL·PMU / bootgen 지식은 그대로 재사용된다.
> 레포는 2026-07-27 `zcu102-coralnpu` 로 이름을 변경했다(이력은 `zcu102-nvdla` 시절 커밋에 보존).

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

---

## 시스템 구성

```
┌─────────────────────── ZCU102 (XCZU9EG) ───────────────────────┐
│                                                                │
│   PS (ARM Cortex-A53)              PL (FPGA)                   │
│   └─ Bare-metal app                └─ Coral NPU                │
│        · W·X·b 주입                      · scalar (RISC-V)     │
│        · Y 수신                          · vector (SIMD)       │
│        · UART 출력                       · matrix (MAC)        │
│              └────────── AXI4 ──────────┘                      │
└────────────────────────────────────────────────────────────────┘
```

리눅스(PetaLinux) 없이 **베어메탈**로 동작. PS가 s_axi(0x5_0000_0000)로 Coral의
ITCM(+0x0) / DTCM(+0x10000) / CSR(+0x30000)에 접근해 프로그램·데이터를 넣고 실행·결과 회수.

---

## 툴체인 흐름

Coral의 RTL은 Verilog가 아니라 **Chisel(Scala)** 로 작성되어 있어,
Vivado에 넣기 전에 Bazel로 Verilog를 생성하는 단계가 필요하다.

```
hdl/chisel (Scala) ─Bazel(WSL2)→ Verilog ─Vivado 2026.1→ bitstream ─bootgen→ BOOT.bin → SD → ZCU102
```

---

## 리포지토리 구조

| 경로 | 내용 |
|---|---|
| `docs/` | 실험 일지(LOG.md), 발표 자료(Coral_NPU_ZCU102.pptx) |
| `01_hw/` | RTL 소스 (coral_axi_wrapper.sv, CoreMiniAxi.sv 등) |
| `03_sw/` | 베어메탈 앱 (main.c — NPU 로더 / FC 레이어 추론) |
| `05_notes/` | 작업 노트, 주차 계획 |
| `05_results/` | 성능·리소스 측정 및 한계 분석 (M6_analysis.md) |
| `06_theory/` | 이론 정리 노트 |

---

## 마일스톤

| | 목표 | 기한 | 상태 |
|---|---|---|---|
| M0 | 보드 동작 검증 (2019.1 prebuilt 부팅) | 07-15 | ✅ |
| M1 | WSL2 + Bazel 환경 구축, 예제 빌드 | 07-22 | ✅ |
| M2 | Verilator 시뮬에서 Coral 실행 + 사이클 측정 | 07-29 | ✅ |
| M3 | Coral RTL → ZCU102 타깃 합성 (bitstream) | 08-05 | ✅ |
| M4 | Block Design + AXI 연결 + 비트스트림 | 08-12 | ✅ |
| M5 | **input → 추론 → output 동작 + SD 부팅** | 08-19 | ✅ |
| M6 | 성능·리소스 측정 및 한계 분석 | 08-26 | ✅ |
| 🎯 | 최종 데모 · 보고 | 08-31 | 🚧 (동작 확보, 발표 준비) |

---

## 결과 요약

- **PS↔NPU AXI 링크 검증**: A53에서 Coral ITCM/DTCM read/write 일치 확인.
- **NPU 실행**: 작은 RISC-V 프로그램을 ITCM에 적재·실행 (scalar `output=input×3`).
- **신경망 추론**: **Y = ReLU(W·X + b)** 완전연결층 1개를 NPU가 계산.
  예) X=[3,1,2,5] → Y=[16,0,10,113] (기대값 일치, ReLU 동작 확인).
- **SD 독립 부팅**: 컴퓨터·JTAG 없이 BOOT.bin(SD)만으로 자동 실행.
- **MNIST 손글씨 인식** (최종 데모): 학습된 MLP(64-32-10)를 **int8 양자화**해 NPU에서 추론.
  · float32 98.00% → **int8 98.00%** (정확도 손실 0), 가중치 9,640B → **2,368B (4배 절감)**
  · NPU 프로그램은 C로 작성해 riscv gcc(rv32im)로 빌드 (손 어셈블 아님)
  · 보드에서 0~9 손글씨 **10/10 정확 인식**, 추론 1회 17,230 명령어(50MHz 기준 ~345µs)
- **CNN 실행**: Conv(8ch 3×3)+ReLU+MaxPool+FC 구조도 NPU에서 동작 (int8 96.89%, 10/10 정확).
  · 파라미터 810개(FC의 1/3)·가중치 864B이나 연산량은 약 2배 — 메모리/연산 트레이드오프 실측
- 상세 측정·한계는 `05_results/M6_analysis.md`, 발표 자료는 `docs/Coral_NPU_ZCU102.pptx` 참고.

---

## 알려진 리스크 / 한계 (요약)

1. **곱셈기 구현 특성** — 기능은 정상(`mul` 동작 실측 확인)이나, 곱셈이 DSP48E2가
   아닌 LUT 로직으로 구현됨(DSP 6개만 사용) → 면적·속도 효율 개선 여지.
2. **타깃 칩 규모 차이** — Coral 기본 타깃 xcvu13p(~3,780K 셀) 대비 ZU9EG는 ~6배 작음.
   → NPU 코어(CoreMiniAxi)만 분리 합성(LUT 18%).
3. **클럭 상한 50MHz** — 100MHz 타이밍 위반 → 50MHz 하향으로 안정 동작.
4. **ZCU102 공식 미지원** — 사내(Nexus) 보드 전제 → AXI wrapper·XDC 직접 작성.

---

## 참고
- Coral NPU: https://github.com/google-coral/coralnpu
- Coral NPU Datasheet: https://developers.google.com/coral/guides/hardware/datasheet
- (이전) NVDLA: https://github.com/nvdla/hw
