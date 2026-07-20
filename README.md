# zcu102-npu

Xilinx **ZCU102** (Zynq UltraScale+ MPSoC, XCZU9EG) 보드에 오픈소스 NPU를 올려
**input → 추론 → output** 을 실제로 동작시키는 프로젝트.

- 기간: 2026-07-13 ~ 2026-08-31 (교수님 데모)
- 보드: Xilinx ZCU102 **Rev 1.1**
- 대상 NPU: **Google Coral NPU** (RISC-V 기반)
- 상태: 🚧 진행 중 — 빌드 환경 구축 단계

> ⚠️ **방향 전환 기록:** 본 프로젝트는 초기에 NVIDIA **NVDLA (nv_small)** 로 시작했으나,
> 2026-07-15 교수님 지시로 **Google Coral NPU** 로 대상을 변경했다.
> NVDLA 단계에서 확보한 보드 부팅 / FSBL·PMU / bootgen 지식은 그대로 재사용된다.
> 레포 이름(`zcu102-nvdla`)은 이력 보존을 위해 당분간 유지한다.

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

## 시스템 구성 (목표)

```
┌─────────────────────── ZCU102 (XCZU9EG) ───────────────────────┐
│                                                                │
│   PS (ARM Cortex-A53)              PL (FPGA)                   │
│   └─ Bare-metal app                └─ Coral NPU                │
│        · input 주입                      · scalar (RISC-V)     │
│        · 결과 수신                       · vector (SIMD)       │
│        · UART 출력                       · matrix (MAC)        │
│              └────────── AXI4 ──────────┘                      │
└────────────────────────────────────────────────────────────────┘
```

리눅스(PetaLinux) 없이 **베어메탈**로 동작시켜 input/output 확인을 목표로 한다.

---

## 툴체인 흐름

Coral의 RTL은 Verilog가 아니라 **Chisel(Scala)** 로 작성되어 있어,
Vivado에 넣기 전에 Bazel로 Verilog를 생성하는 단계가 필요하다.

```
hdl/chisel (Scala)
      │  Bazel  (WSL2 / Ubuntu 22.04)
      ▼
   Verilog / SystemVerilog
      │  Vivado 2026.1 (Windows)
      ▼
   bitstream (.bit)
      │  bootgen  ← NVDLA 단계에서 습득
      ▼
   BOOT.bin  →  SD카드  →  ZCU102
```

---

## 리포지토리 구조

| 경로 | 내용 |
|---|---|
| `docs/` | 실험 일지(LOG.md), 주차별 기록 |
| `01_hw/` | Vivado 프로젝트, XDC 제약, 리소스 리포트 |
| `02_petalinux/` | (보류) 베어메탈 우선 |
| `03_sw/` | 베어메탈 앱, 빌드 스크립트 |
| `04_models/` | 추론 대상 모델 |
| `05_results/` | 성능·리소스 측정 결과 |

로컬 작업 폴더는 `SOTA/zcutonpu/` 아래 `01~05` 로 별도 정리되어 있다.

---

## 마일스톤

| | 목표 | 기한 | 상태 |
|---|---|---|---|
| M0 | 보드 동작 검증 (2019.1 prebuilt 부팅) | 07-15 | ✅ |
| M1 | WSL2 + Bazel 환경 구축, 예제 빌드 | 07-22 | ✅ |
| M2 | Verilator 시뮬에서 Coral 실행 + 사이클 측정 | 07-29 | 🚧 |
| M3 | Coral RTL → ZCU102 타깃 합성 (bitstream) | 08-05 | ⬜ |
| M4 | BOOT.bin 생성 → 보드 부팅 + Coral 인식 | 08-12 | ⬜ |
| M5 | **input → 추론 → output 동작** | 08-19 | ⬜ |
| M6 | 성능·리소스 측정 및 한계 분석 | 08-26 | ⬜ |
| 🎯 | 최종 데모 · 보고 | 08-31 | ⬜ |

---

## 알려진 리스크

1. **타깃 칩 규모 차이 (가장 큼)**
   Coral의 Vivado 합성 타깃은 `xcvu13p` (Virtex UltraScale+, 약 3,780K 로직 셀)이다.
   ZCU102의 ZU9EG는 약 600K로 **6배 이상 작다.**
   → SoC 전체가 아닌 **NPU 코어만 분리해 합성**하는 접근이 필요할 것으로 보인다.
   → 축소 범위와 그 근거 자체가 분석 결과물이 된다.

2. **ZCU102 공식 지원 부재**
   `fpga/README.md`는 Google 사내 FPGA 보드("Nexus") 기준으로 작성되어 있고,
   비트스트림 로딩에 사내 도구(`nexus_loader`, `zturn`)와 사내 호스트를 사용한다.
   → AXI 연결과 XDC 제약을 직접 작성해야 한다.

3. **Chisel → Verilog 생성 의존성**
   Bazel 빌드가 선행되어야 Vivado 합성이 가능하다.

### 완화 요소
- `.core` 파일에 **`FPGA_XILINX`**, `USE_GENERIC` 파라미터와 `vivado` 합성 타깃이
  이미 정의되어 있어 Xilinx 계열 합성 경로 자체는 존재한다.
- Verilator 시뮬레이션 경로가 잘 정비되어 있어,
  합성이 막히더라도 **시뮬레이션 기반 검증·분석**으로 결과물 확보가 가능하다.

---

## 개발 환경

| | 버전 | 비고 |
|---|---|---|
| Host OS | Windows 10 + **WSL2 (Ubuntu 22.04)** | Bazel은 리눅스에서 실행 |
| Vivado | 2026.1 | Windows 측에서 합성 |
| Bazel | 8.6.0 (`.bazelversion`) | bazelisk로 자동 관리 |
| Python | 3.10 | Coral 요구: 3.9–3.12 |
| 부팅 모드 | SD (SW6: 1-ON, 2/3/4-OFF) | |
| UART | J83, 115200 8N1, **CP2108 Interface 0** | 최저 COM 번호 아님에 주의 |

---

## 이전 단계(NVDLA)에서 확보한 것

방향 전환 이후에도 유효한 자산:

- ZCU102 부팅 절차 (SW6 설정, SD 부팅, UART 콘솔)
- **부팅 5단계 이해**: BootROM → FSBL → PMU/ATF → U-Boot → Linux
- **BOOT.bin 구조와 `.bif` 작성법** (bootgen)
- Rev 1.1 보드에서 구버전 FSBL이 실패하는 현상과 그 원인
  (ITRI 배포본의 2018.3 FSBL, `[pmufw_image]` 항목 누락)

이 지식은 M4(BOOT.bin 생성) 단계에서 그대로 사용된다.

---

## 참고

- Coral NPU: https://github.com/google-coral/coralnpu
- Coral NPU Datasheet: https://developers.google.com/coral/guides/hardware/datasheet
- (이전) NVDLA: https://github.com/nvdla/hw
