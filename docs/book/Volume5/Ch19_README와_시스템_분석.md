# Chapter 19. README와 시스템 분석 — 문장마다 해설 달기

> 목표 검증의 장. "README의 모든 문장을 설명할 수 있어야 한다"를 여기서 실행한다. 각 문장 옆 괄호가 근거 장.

---

## 19.1 README 헤더부 해설

> **"Xilinx ZCU102 (Zynq UltraScale+ MPSoC, XCZU9EG) 보드에 오픈소스 NPU를 올려 input → 추론 → output 을 실제로 동작시키는 프로젝트"**

- ZCU102 = 보드, XCZU9EG = 칩, MPSoC = CPU+FPGA 동거 (14장)
- "올린다" = 비트스트림으로 PL 재구성 (13장)
- "추론" = 고정 가중치로 답 계산, 학습 아님 (1장, 4장)

> **"대상 NPU: Google Coral NPU (RISC-V 기반)"**

- RISC-V = 개방형 ISA. 구글이 확장을 자유롭게 넣기 위한 선택 (5~6장)
- NPU인데 "프로세서"인 이유 = Scalar가 폰 노이만 사이클 수행 (2장, 17장)

> **"ISA: rv32imf_zve32x_zicsr_zifencei_zbb"**

- 완전 해독: 32비트, 기본정수(i), **곱셈나눗셈(m)**, 부동소수점(f), 임베디드 벡터(zve32x), CSR 접근(zicsr), 명령 동기화(zifencei), 비트조작(zbb) (6장)

> **"파이프라인: 4-stage, in-order dispatch / out-of-order retire"**

- 4단 파이프라인 + 순차 발행 + 비순차 완료의 면적-성능 절충 (9~10장)

> **"메모리: ITCM 8KB / DTCM 32KB (single-cycle SRAM)"**

- TCM = 캐시 아닌 보장형 1클럭 메모리. NPU의 정적 접근 패턴에 적합 (4장)
- 합성 검산: 40KB ≈ BRAM 10개 ✅ (12장)

> **"버스: AXI4 manager + subordinate"**

- 양방향이라 "ARM이 시키고 Coral이 스스로 데이터를 가져오는" 구조 성립 (15장, 17장)

> **"AXI4 인터페이스를 manager/subordinate 양쪽으로 제공한다는 점이 중요하다"**

- subordinate만 있으면 ARM이 떠먹여야 하고, manager만 있으면 시작 명령을 못 받음 (15장)

## 19.2 툴체인 문단 해설

> **"Coral의 RTL은 Verilog가 아니라 Chisel(Scala)로 작성되어 있어, Vivado에 넣기 전에 Bazel로 Verilog를 생성하는 단계가 필요하다"**

- RTL(18장), Chisel=생성기(18장), Bazel=빌드 그래프(18장), Vivado 3단 공정(13장)
- 흐름: chisel → Bazel(WSL2) → .sv → Vivado(Win) → .bit → bootgen → BOOT.bin → SD (2·13·16장)

## 19.3 리스크 문단 해설

> **"Coral의 Vivado 합성 타깃은 xcvu13p... ZCU102의 ZU9EG는 약 600K로 6배 이상 작다 → NPU 코어만 분리해 합성하는 접근"**

- 착시의 정체: xcvu13p 타깃은 SoC 전체(UART/ISP/ROM 포함) 기준. CoreMiniAxi만 추리면 LUT 18% (17장)
- 교훈: 남의 part 설정이 아니라 내 필요 부분의 크기를 실측하라 (12장)

> **"fpga/README.md는 Google 사내 FPGA 보드('Nexus') 기준... AXI 연결과 XDC 제약을 직접 작성해야 한다"**

- XDC = 클럭·핀 제약 명세 (13장). Block Design 자동화로 상당 부분 대체 (15장)

> **"Verilator 시뮬레이션 경로가 잘 정비되어 있어... 시뮬레이션 기반 검증·분석으로 결과물 확보 가능"**

- 안전망 논리: 최선(보드 동작) / 차선(합성+분석) / 최소(시뮬+사이클) 3단 확보 전략

## 19.4 main.c 뼈대 분석 (M4~M5용 설계)

```c
#include "xparameters.h"
#include "xil_io.h"
#include "xil_printf.h"

#define CORAL_BASE   XPAR_CORALNPU_0_BASEADDR
#define DDR_IN_BUF   0x10000000    // 입력 버퍼 (DDR 내 예약)
#define DDR_OUT_BUF  0x10100000    // 출력 버퍼

extern const unsigned char coral_prog[];   // RISC-V 바이너리 (objcopy로 C배열화)
extern const unsigned int  coral_prog_len;

int main(void) {
    xil_printf("== Coral bring-up ==\r\n");

    // 1) 리셋 상태에서 ITCM 적재
    coral_hold_reset();                              // 제어 레지스터로 리셋 유지
    axi_memcpy(CORAL_ITCM_ADDR, coral_prog, coral_prog_len);

    // 2) 입력 데이터 준비 (Coral 프로그램과 약속된 주소)
    prepare_input((void*)DDR_IN_BUF);
    Xil_DCacheFlush();                               // ★ 캐시 내용을 DDR로 밀어냄

    // 3) 시작
    coral_release_reset();

    // 4) 완료 폴링
    while (Xil_In32(CORAL_BASE + DONE_REG) == 0);

    // 5) 결과 회수
    Xil_DCacheInvalidateRange(DDR_OUT_BUF, OUT_LEN); // ★ 낡은 캐시 무효화
    dump_result((void*)DDR_OUT_BUF);
    while (1);
}
```

핵심 줄 해설:
- `Xil_DCacheFlush / InvalidateRange` — **베어메탈 최대 함정.** A53은 캐시(4장)를 쓰므로, ARM이 쓴 데이터가 DDR에 실제로 내려가 있지 않으면 Coral(master 포트로 DDR 직접 접근)은 낡은 값을 읽습니다. 반대로 Coral이 쓴 결과는 ARM 캐시에 없으므로 무효화 후 읽어야 합니다. **HP 포트는 캐시 일관성이 없기 때문** (HPC 포트였다면 하드웨어가 처리 — 15장).
- `coral_hold_reset` 중 적재 — 실행 중인 코어의 명령 메모리를 바꾸면 안 되므로 리셋 상태에서 ITCM을 채웁니다 (2장 Fetch 원리).
- "약속된 주소" — ARM 코드와 Coral 프로그램이 **같은 메모리 맵 합의**를 공유해야 합니다 (4장).

## 19.5 요약·퀴즈

- README의 전 문장이 Vol.1~4 개념으로 환원됨을 확인 — 책의 목표 달성 검증.
- main.c의 실전 함정 1순위는 **캐시 일관성**(Flush/Invalidate). 2순위는 리셋 중 적재 순서.

**Q (교수님). "ARM이 준비한 입력을 Coral이 못 읽는다. 배선은 정상. 무엇부터 보나?"**
A. 캐시 일관성입니다. HP 포트는 비일관성 경로이므로 ARM의 DCache Flush 누락 시 데이터가 DDR에 내려가지 않습니다. Flush 추가 후에도 실패하면 주소 합의(메모리 맵)와 Address Editor 배정을 대조합니다.

**퀴즈**: ① 결과 읽기 전 InvalidateRange가 필요한 이유? ② ITCM 적재를 리셋 중에 하는 이유?
<details><summary>정답</summary>① Coral이 DDR에 쓴 새 값 대신 ARM 캐시의 낡은 값을 읽지 않기 위해 ② 실행 중인 코어의 명령 메모리 변경을 피하기 위해</details>

---
**다음 장**: 성능 분석 — Shift+Add, M Extension, DSP, Vector의 4중주.
