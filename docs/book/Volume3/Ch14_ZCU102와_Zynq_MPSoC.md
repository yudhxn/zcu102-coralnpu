# Chapter 14. ZCU102와 Zynq MPSoC — 두 세계가 한 칩에

---

## 14.1 보드와 칩 구분

**ZCU102** — Xilinx의 **평가 보드**(개발 키트). 기판 전체. 전원, DDR4, SD 슬롯, UART, 스위치가 붙어 있음.
**XCZU9EG** — 그 위의 **칩.** Zynq UltraScale+ MPSoC 계열. 진짜 두뇌.

**Zynq** /zɪŋk/ 징크 — Xilinx의 "CPU+FPGA 통합 칩" 브랜드.
**MPSoC** /ˌɛmpiːˌɛsoʊˈsiː/ — Multi-Processor System on Chip. 여러 종류의 프로세서 + FPGA를 한 실리콘에.

### 왜 이런 칩이 나왔나

2010년대 초, 임베디드 시스템은 대부분 "CPU 칩 + FPGA 칩" 2칩 구성이었습니다. 칩 사이 통신이 느리고 보드가 복잡했죠. Xilinx의 해답: **한 칩에 넣고 내부 버스로 직결하자.** 그것이 Zynq(2011)이고, 후속 세대가 Zynq UltraScale+ MPSoC입니다.

## 14.2 PS와 PL

```
┌────────────────── XCZU9EG ──────────────────┐
│                                              │
│  PS (Processing System)  │  PL (Programmable │
│  = 굳은 회로 (ASIC 부분)  │       Logic)      │
│                          │  = FPGA 부분      │
│  · APU: Cortex-A53 ×4    │                   │
│    (64b, 리눅스급)        │  · LUT 274,080    │
│  · RPU: Cortex-R5 ×2     │  · FF  548,160    │
│    (실시간용)             │  · BRAM 912       │
│  · PMU: 전원 관리         │  · DSP 2,520      │
│  · CSU: 보안/부팅         │                   │
│  · DDR 컨트롤러, UART,    │  ← Coral NPU가    │
│    SD, GbE, USB ...      │     여기 들어감    │
│                          │                   │
│        └──── AXI 포트들로 상호 연결 ────┘     │
└──────────────────────────────────────────────┘
```

**PS** = 11장의 ASIC 세계. 빠르고 효율적이지만 고정.
**PL** = 11~13장의 FPGA 세계. 유연하지만 느림.

> **이 칩의 존재 이유**: 제어·OS·통신처럼 "굳혀도 되는 것"은 PS에서 효율적으로, 가속기처럼 "바꿔가며 실험할 것"은 PL에서 유연하게. **우리 프로젝트가 이 분업의 교과서적 사례**입니다 — 베어메탈 제어는 PS(A53), Coral은 PL.

### PS 조연들

- **APU** (Application Processing Unit): Cortex-A53 4코어. 베어메탈 앱이 a53-0에서 돎.
- **RPU** (Real-time PU): Cortex-R5. 이번 프로젝트 미사용.
- **PMU** (Platform Management Unit): 전원 도메인 관리. **부팅 필수 참가자** (16장의 `[pmufw_image]` 사건 주인공).
- **CSU** (Configuration & Security Unit): BootROM 실행, 부팅 이미지 검증.

## 14.3 ZCU102 보드 실무 정보

| 항목 | 값 | 비고 |
|---|---|---|
| 부팅 모드 | SW6 = **1-ON, 2/3/4-OFF** | SD 부팅 |
| UART | J83, **115200 8N1** | 콘솔 |
| USB-시리얼 | CP2108, **Interface 0** | ⚠️ COM 번호 최저 아님 (실측 COM6) |
| DDR | PS측 DDR4 SODIMM | FSBL이 초기화 |
| 보드 리비전 | **Rev 1.1** | 구버전 부팅 이미지와 비호환 이력 |

> **Rev 1.1 교훈**: 보드 리비전에 따라 탑재 부품·실리콘이 달라, 옛 리비전용으로 빌드된 부팅 코드가 실패할 수 있습니다. 우리는 ITRI 배포본(구형 기준)이 멈추는 것을 겪었고, 공식 2019.1 프리빌트로 보드 무결성을 입증한 뒤 원인을 소프트웨어로 좁혔습니다. — "하드웨어 탓인지 소프트웨어 탓인지"를 **대조 실험으로 분리**한 사례.

## 14.4 현재 프로젝트 연결

- 시스템 구성 그림 그 자체입니다: **PS(베어메탈 앱: input 주입, 결과 수신, UART 출력) ↔ AXI ↔ PL(CoreMiniAxi)**.
- PL 자원표(LUT 274K 등)가 12장 리포트의 "가용" 열의 출처.
- PS의 DDR 컨트롤러가 PS 소속이라는 점이 중요: **Coral(PL)이 DDR을 쓰려면 반드시 PS를 거쳐야** 하고, 그 통로가 S_AXI_HP0(15장).

## 요약·퀴즈

- ZCU102(보드) ≠ XCZU9EG(칩). 칩은 PS(ASIC적 CPU 세계) + PL(FPGA 세계)의 동거.
- 분업 원칙: 굳혀도 되는 것 → PS, 실험할 것 → PL. 우리 프로젝트가 그 표본.
- 보드 실무: SW6, J83/115200, CP2108 Interface 0(≠최저 COM), Rev 1.1 호환성 이력.

**퀴즈**: ① Coral이 DDR에 접근할 때 반드시 거치는 쪽은? ② PMU의 풀네임과 역할은?
<details><summary>정답</summary>① PS (DDR 컨트롤러가 PS 소속) ② Platform Management Unit, 전원 도메인·초기화 관리(부팅 필수)</details>

---
**다음 장**: AXI4 — 두 세계를 잇는 고속도로.
