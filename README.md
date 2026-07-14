# zcu102-nvdla

ZCU102 (Zynq UltraScale+ MPSoC, XCZU9EG) 보드에 오픈소스 NPU **NVDLA (nv_small)** 를 올리고,
CNN 추론을 수행 · 분석하는 프로젝트.

- 기간: 2026-07-13 ~ 2026-08-31
- 보드: Xilinx ZCU102
- NPU: NVDLA `nv_small` (INT8, 64 MAC)
- 상태: 🚧 진행 중

---

## 왜 nv_small 인가
`nv_full` 구성은 ZCU102의 LUT 용량을 초과한다. `nv_small`(INT8, 64 MAC)이
ZCU102에 실제로 구현·검증된 사례가 있는 유일한 실전 구성이다.

## 시스템 구성

```
┌─────────────────────── ZCU102 (XCZU9EG) ───────────────────────┐
│                                                                │
│   PS (ARM Cortex-A53)              PL (FPGA)                   │
│   ├─ PetaLinux                     └─ NVDLA nv_small           │
│   ├─ opendla.ko (KMD)                 ├─ CSB ← AXI-Lite        │
│   └─ nvdla_runtime (UMD)              └─ DBB ← AXI HP port     │
│                                                                │
└────────────────────────────────────────────────────────────────┘
        ▲
        │ .nvdla loadable
   nvdla_compiler (host)  ←  Caffe model + INT8 calibration table
```

## 디렉토리

| 경로 | 내용 |
|---|---|
| `01_hw/` | NVDLA RTL 생성 스크립트, Vivado 프로젝트 tcl, 리소스 리포트 |
| `02_petalinux/` | PetaLinux 설정, device tree, 부팅 이미지 빌드 스크립트 |
| `03_sw/` | KMD/UMD 빌드, 캘리브레이션 툴 |
| `04_models/` | prototxt / caffemodel / calibration table / .nvdla loadable |
| `05_results/` | 성능·정확도 측정 결과, 로그, 그래프 |
| `docs/` | 주차별 기록, 발표자료 |

## 마일스톤

| | 목표 | 기한 | 상태 |
|---|---|---|---|
| M1 | 사전빌드 BOOT.BIN으로 보드 부팅 | 07-17 | ⬜ |
| M2 | 사전빌드 loadable로 첫 추론 성공 | 07-24 | ⬜ |
| M3 | nv_small RTL 직접 합성 → bitstream | 07-31 | ⬜ |
| M4 | 내 bitstream + PetaLinux + opendla.ko 부팅 | 08-07 | ⬜ |
| M5 | nvdla_compiler로 모델 직접 컴파일 → 실행 | 08-14 | ⬜ |
| M6 | (스트레치) 자체 INT8 캘리브레이터 | 08-21 | ⬜ |
| 🎯 | 최종 데모 · 보고 | 08-31 | ⬜ |

## 환경

| | 버전 | 비고 |
|---|---|---|
| Host OS | Ubuntu 22.04 | PetaLinux는 리눅스 전용 |
| Vivado | (레퍼런스 = 2019.1) | 최신 버전은 KMD 호환성 깨질 수 있음 |
| PetaLinux | 2019.1 | |
| 보드 부팅 | SD (SW6: 1-ON, 2/3/4-OFF) | |
| UART | J83, 115200 8N1, 최저번호 COM 포트 | |

## 참고

- NVDLA HW: https://github.com/nvdla/hw
- NVDLA SW (컴파일러/런타임): https://github.com/nvdla/sw
- ITRI-OpenDLA (ZCU102 사전빌드 이미지)
- ZYNQ-NVDLA (Zynq 포팅 레퍼런스)
