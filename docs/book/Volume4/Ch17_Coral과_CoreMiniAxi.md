# Chapter 17. Google Coral과 CoreMiniAxi — NPU 해부

---

## 17.1 Google Coral의 역사

**Coral** /ˈkɔːrəl/ 코럴 — 구글의 엣지 AI 브랜드. 1세대(2019)는 Edge TPU라는 **닫힌 ASIC**을 파는 제품군이었습니다. 2025년 10월, Google Research/DeepMind가 방향을 바꿔 **Coral NPU를 오픈소스 RTL로 공개**합니다.

### 왜 공개했는가

엣지 AI 칩 시장은 파편화가 심합니다. 구글의 계산: 설계를 공개해 **생태계 표준**을 노리고(RISC-V 진영과 동맹), 연구자·칩 회사가 가져다 쓰게 하는 것. 우리가 ZCU102에 올릴 수 있는 것도 이 공개 덕분입니다.

## 17.2 세 개의 유닛 — 왜 셋인가

딥러닝 추론 코드를 뜯어보면 연산이 세 부류로 나뉩니다:

```
① 제어 흐름: 반복문, 주소 계산, 분기        → 소량, 순차적
② 원소별 연산: 활성화 함수, 정규화           → 같은 일을 데이터마다
③ 행렬 곱셈: 가중치×입력                    → 연산량의 압도적 대부분
```

한 종류의 회로로 셋을 다 하면 비효율입니다. Coral의 답: **부류마다 전담 유닛.**

```
┌───────────────── Coral NPU ─────────────────┐
│                                              │
│  Scalar Unit ── ①담당. RV32I+M+... 실행     │
│    (Vol.1의 CPU 그 자체. 4단 파이프라인,     │
│     in-order dispatch / OoO retire)          │
│                                              │
│  Vector Unit ── ②담당. SIMD 128b           │
│    (한 명령으로 8b×16개 동시 처리.           │
│     ISA 근거: zve32x 확장)                   │
│                                              │
│  Matrix Unit ── ③담당. MAC 배열            │
│    (1장의 "방법 B"를 대량 배치)              │
│                                              │
│  ITCM 8KB ── 명령어 전용, 1클럭 (Fetch용)   │
│  DTCM 32KB ── 데이터 전용, 1클럭            │
│  AXI4 manager + subordinate ── 바깥 통로    │
└──────────────────────────────────────────────┘
```

### 왜 Vector로는 부족해서 Matrix까지 있나

행렬곱은 데이터 재사용이 극단적입니다(같은 가중치 행이 여러 입력 열과 곱해짐). SIMD는 "한 줄 동시 처리"까지만 되지만, MAC **2차원 배열**은 데이터를 격자 안에서 돌려쓰며 곱셈-누적을 면적당 최대로 뽑습니다. 연산량의 대부분이 ③이므로 전용 배열이 정당화됩니다.

## 17.3 CoreMiniAxi — 우리가 합성한 것의 정체

Coral 레포는 여러 최상위 구성을 제공합니다:

```
전체 SoC (fpga/coralnpu_soc.core)
  · NPU + UART + I2C + ISP + ROM + 버스 전부 포함
  · 합성 타깃: xcvu13p (≈3,780K LC) ← ZU9EG의 6배
  
CoreMiniAxi  ← ★ 우리가 선택
  · NPU 코어 + TCM + AXI 인터페이스만
  · 실측: ZU9EG의 LUT 18%
```

> **M3의 핵심 판단 복습**: "6배 커서 불가능"은 SoC 전체 기준의 착시였고, 코어만 분리하니 넉넉히 들어갔다. 남의 설정값이 아니라 **내가 필요한 부분의 크기**를 봐야 한다.

### 포트 명세 (M4 설계 근거)

| 포트 | 의미 |
|---|---|
| `io_aclk`, `io_aresetn` | 단일 클럭·단일 리셋 (CDC 문제 없음) |
| `io_axi_slave_*` | addr 32b / data 128b. **ARM→Coral 제어** (ITCM 적재, 시작 명령) |
| `io_axi_master_*` | addr 32b / data 128b. **Coral→DDR 데이터** |
| `io_irq` 계열 | 완료 알림 (폴링 대신 쓸 수 있는 인터럽트) |

### 실행 모델 — 한 번의 추론이 흘러가는 길

```
1. ARM이 slave 포트로 ITCM에 RISC-V 바이너리 적재
2. ARM이 DDR에 입력 데이터 준비
3. ARM이 제어 레지스터에 "시작" (리셋 해제)
4. Scalar가 Fetch 시작 → 프로그램이 Vector/Matrix 구동
5. master 포트로 DDR에서 입력·가중치 로드 (DTCM 경유)
6. 결과를 DDR에 저장, 완료 표시
7. ARM이 폴링(또는 IRQ)으로 감지 → 결과 읽어 UART 출력
```

Vol.1~3의 전 개념이 이 일곱 줄에 다 들어 있습니다: 폰 노이만(1,4) / TCM(1,5) / AXI 양방향(1,5,7) / MMIO 베어메탈(3,7).

## 17.4 요약·퀴즈

- Coral = 2025년 공개된 오픈소스 RISC-V NPU. Scalar(제어)+Vector(SIMD)+Matrix(MAC 배열)의 3분업.
- CoreMiniAxi = 코어만 추린 최상위 구성. 단일 클럭, AXI 128b 양방향. ZU9EG에 LUT 18%.
- 실행 모델: ARM이 적재·시작 → Coral이 자율 실행 → 완료 통지.

**Q (교수님). "Vector와 Matrix Unit의 역할 차이는?"**
A. Vector는 SIMD로 원소별 연산(활성화 등)을 병렬화하고, Matrix는 MAC 2차원 배열로 행렬곱의 데이터 재사용을 극대화합니다. 연산량 대부분이 행렬곱이므로 별도 배열이 면적 대비 효율적입니다.

**퀴즈**: ① CoreMiniAxi에 포함되지 않는 것(UART/TCM/AXI 중)? ② Coral 실행의 시작 신호를 보내는 주체는?
<details><summary>정답</summary>① UART (SoC 구성에만 있음) ② ARM(PS)이 slave 포트의 제어 레지스터로</details>

---
**다음 장**: Chisel · Scala · Bazel · RTL — 이 설계는 어떻게 쓰였나.
