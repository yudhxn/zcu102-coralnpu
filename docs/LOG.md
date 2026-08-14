

---



### 2026-07-15 (수)  — Coral NPU로 방향 전환

- **대상 NPU 변경: NVDLA → Google Coral NPU**
- 목표: ZCU102에 Coral을 올려 input → 추론 → output 동작

[Coral NPU 팩트체크]
- Google Research/DeepMind 공동 설계, 2025년 10월 공개 오픈소스 NPU
- 32-bit RISC-V ISA 기반, matrix + vector(SIMD) + scalar 3부 구성
- 레포 활발 (커밋 1,403 / 별 2.4k), fpga·platforms·hdl 폴더 존재
- 매트릭스 코어 릴리스 확인 (M3-2026-04-27)
- 빌드 = Bazel, 언어 = SystemVerilog/Scala(Chisel), Verilator 시뮬 가능

[판단]
- 불가능하지 않음. 단 NVDLA보다 어렵고 ZCU102 레퍼런스가 없음
- 안전망: Verilator 시뮬 진입이 쉬워 최악의 경우 시뮬 검증까지는 확보 가능
- 재활용 가능한 자산: 보드 부팅 절차 / FSBL·PMU / bootgen / 부팅 5단계 원리
  → 특히 BOOT.bin 생성 단계는 이미 절반 습득한 상태

---

### 2026-07-20 (월) — 빌드 환경 구축 및 시뮬레이션 실행 성공

[선배 확인]
- 리눅스 서버 불필요. 윈도우 Vivado 2026.1로 진행
- 목표 수준: "보드에서 input/output만 되면 된다" → 베어메탈 방식으로 확정

[Coral 레포 조사]
- `fpga/README.md` → **Google 사내 보드("Nexus") 전용 문서**
  · nexusXX.mtv.corp.google.com 접속, `nexus_loader` / `zturn` 등 비공개 사내 도구 사용
  · ZCU102 포함 공개 보드 지원 없음 → AXI 연결·XDC 직접 작성 필요
- `hdl/verilog/` 에는 Sram.v, ClockGate.sv, RstSync.sv 등 부품 셀만 존재
  · NPU 본체는 `hdl/chisel` (Scala) → Bazel로 Verilog 생성 단계 필요
- `fpga/coralnpu_soc.core` 분석
  · vivado 합성 타깃 존재, `FPGA_XILINX` / `USE_GENERIC` 파라미터 있음 (긍정)
  · part = `xcvu13p-fhga2104-2-e` (Virtex UltraScale+, 약 3,780K LC)
  · ZCU102(ZU9EG)는 약 600K → **6배 이상 작음.** SoC 전체 합성 불가 예상
  · 대응: NPU 코어만 분리 합성

[환경 구축]
- **WSL2 + Ubuntu 22.04** 설치 (윈도우 유지, 듀얼부팅 아님)
  · 초기 OOBE 멈춤 → `wsl --unregister` 후 재설치로 해결
- build-essential / git / python3(3.10) / srecord / curl / zip / unzip
- bazelisk → `/usr/local/bin/bazel`
- coralnpu clone (리눅스 홈 `~/coralnpu`. 윈도우 경로는 빌드 속도 문제로 회피)
- `.bazelversion` = **8.6.0** (README의 7.4.1과 상이하나 bazelisk가 자동 처리)

[빌드 & 실행 — 성공]
1. 예제 빌드 (222s)
   `bazel build //examples:coralnpu_v2_hello_world_add_floats`
   → .elf / .bin / .vmem 생성
2. 시뮬레이터 빌드 (368s)
   `bazel build //tests/verilator_sim:core_mini_axi_sim`
   → 로그에 `core_mini_axi_cc_library_emit_verilog` 확인
   → **Chisel → Verilog 변환이 실제로 수행됨**
   → Verilator: 124 modules, 1.592 MB sources (합성 재료 확보)
3. 시뮬레이션 실행 성공
   → "Simulation stopped by user" 정상 종료 (core dump 없음)
   → **Coral NPU RTL이 로컬에서 RISC-V 바이너리를 실제 실행함**

[삽질 기록 — 경로 문제]
- examples(RISC-V)와 verilator_sim(x86)이 서로 다른 빌드 설정
  → 하나를 빌드하면 `bazel-bin` 심볼릭 링크가 그쪽으로 이동,
    다른 하나가 "No such file or directory"로 사라짐
- 출력 경로가 서로 다름:
  · 시뮬레이터: `bazel-out/k8-fastbuild/`
  · ELF:        `bazel-out/k8-fastbuild-ST-dd8dc713f32d/`
- 해결: 양쪽 모두 **절대 경로**로 지정. `~/.bashrc`에 `$SIM` / `$ELF` / `$RTL` 등록

[결과] ✅ M1 완료 / ✅ M2 진입

---

### 2026-07-20 (월) — M3 달성: Coral NPU ZCU102 합성 성공 ✅

[합성 조건]
- 도구: Vivado 2026.1 (Windows)
- 타깃: `xczu9eg-ffvb1156-2-e` (ZCU102 Evaluation Board)
- Top module: **CoreMiniAxi**
- 소스: `CoreMiniAxi.sv` 단일 파일 (36,725줄 / 1.6MB)
  · Chisel → Verilog 생성물
  · ClockGate, RstSync, Sram 모듈이 모두 내장되어 있어 추가 파일 불필요
    (별도 파일을 함께 추가하면 module redefinition 오류)
- Verilog define: **`SYNTHESIS`**
  · 이 define이 SRAM 구현 분기를 결정함
  · 정의 시 → 합성 가능한 동작 기술 메모리(`bit [127:0] mem[...]`) → BRAM 추론
  · 미정의 시 → DPI-C 기반 시뮬 전용 메모리 (합성 불가)
  · USE_TSMC12FFC / USE_GF12LPP / USE_GF22 는 실칩 공정 매크로이므로 정의하지 않음

[리소스 사용량 — 핵심 결과]

| 자원        | 사용    | 가용     | 비율   |
|-------------|---------|----------|--------|
| CLB LUT     | ~49,300 | 274,080  | ~18%   |
| Register    | ~8,930  | 548,160  | ~1.6%  |
| Block RAM   | 10      | 912      | 1.10%  |
| DSP48E2     | 6       | 2,520    | 0.24%  |
| BUFGCE      | 3       | 116      | 2.59%  |

**→ Coral NPU 코어는 ZCU102에 충분히 수용 가능.**

[리스크 해소]
- 사전 우려: 합성 타깃이 xcvu13p(약 3,780K LC)로 ZCU102의 6배 → 수용 불가 예상
- 실제 결과: **기우였음.** 해당 타깃은 SoC 전체(UART/I2C/ISP/ROM/버스 포함) 기준이었고,
  NPU 코어(CoreMiniAxi)만 분리하면 LUT 18% 수준
- 결론: **CoreMiniAxi 단독 분리 전략의 유효성이 정량적으로 입증됨**

[Bonded IOB 873.78% — 문제 아님]
- 원인: CoreMiniAxi를 최상위로 단독 합성하여 AXI 신호 2,866개가
  모두 외부 물리 핀(IOB)으로 매핑됨. ZCU102 IOB는 328개
- M4에서 PS(ARM)와 AXI로 내부 연결하면 칩 내부 배선이 되어 IOB를 소비하지 않음
- 별도 조치 불필요

[관찰 / 후속 분석 대상]
- DSP를 6개만 사용 → 매트릭스 연산이 DSP48E2가 아닌 LUT로 구현된 것으로 보임
  · USE_GENERIC 경로의 영향 가능성. M6 분석 항목으로 기록
- BRAM 10개 ≈ ITCM 8KB + DTCM 32KB (사양과 일치)
- LUT6 25,975개로 연산 로직이 LUT에 집중

[포트 구조 확인 — M4 설계 근거]
- `io_aclk` / `io_aresetn` : 단일 클럭, 단일 리셋 (설계 단순)
- `io_axi_slave_*`  : addr 32b / data **128b** → PS M_AXI_HPM → Coral 제어
- `io_axi_master_*` : addr 32b / data **128b** → Coral → PS S_AXI_HP (메모리 접근)
- 표준 AXI4 5채널 구조 → ZCU102 HP 포트(128b)와 폭 일치, 변환 로직 불필요

[다음 — M4]
1. Vivado Block Design 생성 (Zynq UltraScale+ PS + CoreMiniAxi)
2. PS M_AXI_HPM0 → Coral slave 연결
3. Coral master → PS S_AXI_HP0 연결
4. Implementation → bitstream 생성
5. Vitis 베어메탈 앱: input 주입 → output 확인 (UART)
6. BOOT.bin 생성 (NVDLA 단계에서 습득한 bootgen 활용)


### 2026-07-21 (화) — M4: Block Design + 비트스트림 생성 성공 ✅

[목표] ZCU102 PS(ARM)와 Coral NPU를 AXI로 연결 → 비트스트림 생성

[AXI 이름 불일치 문제와 해결]
- Coral(CoreMiniAxi)의 포트가 Chisel식 이름(io_axi_slave_write_addr_ready 등)이라
  Vivado가 AXI 인터페이스로 자동 인식하지 못함
- IP 패키징의 Auto Infer Interface도 실패 ("No interface was inferred")
- **해결: coral_axi_wrapper.sv 작성**
  · io_axi_slave_* → s_axi_* , io_axi_master_* → m_axi_* 로 표준 이름 변환
  · 미사용 포트(io_debug_*, io_dm_*, io_wfi 등)는 open/상수 고정
  · io_boot_addr, io_irq만 외부 노출
  · Open Elaborated Design으로 문법·연결 검증 통과
  → wrapper 적용 후 Vivado가 s_axi / m_axi 를 AXI로 자동 인식 (핵심 돌파)

[Block Design 구성]
- Zynq UltraScale+ MPSoC (PS) + coral_axi_wrapper + AXI SmartConnect ×2 + Proc Reset
- 연결:
  · PS M_AXI_HPM → SmartConnect → wrapper s_axi  (PS가 Coral 제어)
  · wrapper m_axi → SmartConnect → PS S_AXI_HP0   (Coral이 메모리 접근)
  · 클럭/리셋 자동 연결 (Run Connection Automation)
  · irq/boot_addr는 Constant(0)로 고정
- irq 자동 인터럽트 연결은 실패하여 제외 (Connection Automation에서 체크 해제)

[삽질 기록]
- Add Module로 wrapper 추가 시 "Module references are still updating" 반복
  → Tcl로 우회: create_bd_cell -type module -reference coral_axi_wrapper ...
- 첫 Generate Bitstream에서 IO Placement 실패
  → "870 I/O ports > 707 available" 에러
  → 원인: Top이 coral_axi_wrapper(내부)로 잡혀 AXI 신호가 물리 핀으로 노출됨
  → 해결: Top을 coralnpu_wrapper(Block Design 전체 래퍼)로 변경
     → AXI가 PS와 내부 연결되어 물리 핀 미사용, 정상 진행

[결과]
- Validate Design: 에러/critical warning 없음 통과
- Synthesis → Implementation → **write_bitstream Complete** ✅
- 비트스트림 생성 성공 (coralnpu_wrapper.bit)

[남은 경고 — 내일 처리]
- ⚠️ Timing 미충족 (Timing 38-282)
  → Coral 클럭이 높아 타이밍 위반. M5 보드 테스트에서 문제 시 클럭 하향(예: 100→50MHz)
- ⚠️ Export Hardware에서 "Failed to write hardware handoff data" critical warning
  → XSA에 PS handoff 데이터 누락. Vitis 진행 전 해결 필요
  → 대응 예정: Generate Block Design 재실행 → 저장 → Export Hardware 재시도

[다음 — M5]
1. XSA 정상 내보내기 (handoff 경고 해결)
2. Vitis 실행 → 베어메탈 앱
3. Coral에 input 주입 → output 확인 (UART)
4. Timing 문제 시 클럭 조정

### 2026-07-23 (목) — M4 마무리 + M5 착수: XSA·Vitis·보드 동작·PS↔NPU AXI 링크 검증 ✅

[목표] 어제 남긴 타이밍 위반 해결 → XSA 내보내기 → Vitis 베어메탈 → 보드에서 PS↔Coral AXI 통신 확인

[타이밍 위반 해결 — 클럭 하향]
- 어제 남긴 Timing 미충족(WNS -2.385, TNS -3363) 처리
- ZYNQ PS의 PL_CLK0을 100 → 50MHz로 하향 (Clock Configuration → PL Fabric Clocks)
  · 클럭 주기 10ns → 20ns → 여유 시간 10ns 확보
- 재합성 → 재구현 후 **WNS +0.462 / TNS 0.000 → 타이밍 통과** ✅
- 데모 목표(input→output 동작 확인)에는 속도 무관하므로 클럭 하향이 가장 안전

[XSA 내보내기]
- Export Hardware (Include bitstream) → coralnpu_wrapper.xsa 정상 생성
- 어제의 "hardware handoff data" critical warning은 XSA 생성에 영향 없음 (무해로 판명)
- 경로 주의: 한글·공백 없는 경로 권장 (Vitis가 못 읽는 경우 있음)

[Vitis 플랫폼/앱 구성 — Vitis Unified IDE 2026.1]
- Platform(coral_platform): XSA 기반, OS **standalone**, CPU **psu_cortexa53_0**
- Application(coral_app): Empty Application → main.c 직접 작성
- 삽질: main.c를 실수로 platform 쪽에 생성 → 빌드 시 `ninja: no work to do`
  → coral_app/Sources로 옮겼으나 이번엔 `undefined reference to 'main'`
  → 빈 main.c가 컴파일된 것이 원인, 코드 다시 넣고 저장하여 해결
- Build Finished successfully → **coral_app.elf** 생성

[보드 부팅 + UART 확인]
- SW6 JTAG 모드, USB-UART 연결, PuTTY 115200 8N1 (COM6, Silicon Labs)
- 삽질: Vitis 내장 시리얼 터미널 "Access denied" (포트 점유) → PuTTY로 우회
- Hello World 앱 정상 출력 → **PS 베어메탈 실행 경로 검증** ✅

[PS↔Coral AXI 링크 검증 — 핵심 돌파]
- Vivado Address Editor 확인: coral s_axi(reg0) = 0x5_0000_0000, range 4G
- A53에서 Coral 내부 메모리 read/write 테스트 (Xil_Out32/Xil_In32)
  · ITCM(0x5_0000_0000), DTCM(0x5_0001_0000) 각 4워드 write → read
  · **전부 일치 (PASS)** → PS↔NPU AXI 배선 정상 입증 ✅
- Xil_Out32/Xil_In32는 64-bit 주소(UINTPTR) 그대로 사용 가능 (default MMU가 PL 영역 매핑)

[CoreMiniAxi 제어 규약 파악 — 레퍼런스 드라이버 분석]
- google-coral/coralnpu `core_mini_axi_tb` 분석으로 확보:
  · s_axi 창 내부 맵: **ITCM +0x0, DTCM +0x10000, CSR +0x30000**
  · CSR+0x0 = reset/clock 제어, CSR+0x4 = entry PC, CSR+0x8 = 상태
  · 실행 순서: 프로그램 적재 → CSR+0x4에 entry → ClockGate(false)=0x1 → Reset(false)=0x0 → 실행
  · 완료 감지: CSR+0x8 상태 레지스터 또는 DTCM done 플래그 폴링

[다음 — M5 완성]
1. NPU 실행 로더: 작은 RISC-V 프로그램(기계어 하드코딩)을 ITCM에 적재
   · DTCM에서 input 읽어 계산 → output + done을 DTCM에 기록
2. A53에서 done 플래그 폴링 → output 읽어 UART 출력 (진짜 input→추론→output)
3. 완성 앱으로 BOOT.bin 생성 → SD 부팅 (컴퓨터 없이 데모)


# 2026-07-27 (월) — NPU 신경망 추론 데모 확장: FC층 → XOR(2층) → C 컴파일 파이프라인 + 3층 MLP 🧠

> **한 줄 요약**
> scalar(×3) 데모를 **완전연결층 추론 `Y = ReLU(W·X + b)`** 로 업그레이드하고, 이어서 **2층 신경망(XOR)** 까지 확장했다.
> 도중에 발견한 `mul`(하드웨어 곱셈) 이상 동작은 **통제 실험으로 재검증한 결과 정상**으로 정정했다.
> 마지막으로 손 어셈블의 한계를 넘어 **C → RISC-V(rv32im) 컴파일 파이프라인**을 구축하고 **3층 MLP** 를 준비했다.

---

## 1. 완전연결층(FC) 추론 데모 — `Y = ReLU(W·X + b)`

### 목표
어제까지의 scalar 데모(`output = input × 3`)를 **신경망 한 층(뉴런 4개) 계산**으로 업그레이드.

### 프로그램 설계
- 4×4 가중치 행렬 `W` · 입력벡터 `X(4)` · 편향 `b(4)` → 출력 `Y(4)` 의 완전연결층 1개
- RISC-V로 **이중 반복문(행 `i`, 열 `j`) + MAC 누적 + 편향 가산 + ReLU** 구현
- DTCM 메모리 배치

  | 데이터 | 오프셋 | 크기 |
  |---|---|---|
  | `W` | `+0x00` | 16 word |
  | `X` | `+0x40` | 4 word |
  | `b` | `+0x50` | 4 word |
  | `Y` | `+0x60` | 4 word |
  | `done` | `+0x70` | 1 word |

### 검증 방식
- 로컬에 keystone/unicorn 설치 시도 → **keystone은 RISC-V 미지원** 확인
- 직접 만든 **미니 어셈블러**로 기계어 생성 + **unicorn(RV32) 에뮬레이터**로 실행 검증
- 테스트 케이스 3개 모두 기대값과 일치 → 보드 적용

### 결과 ✅
- `W·X + b` 와 ReLU까지 NPU가 계산
- 예: `X = [3, 1, 2, 5]` → `Y = [16, 0, 10, 113]` (2행은 `-45` → ReLU → `0`)
- A53(호스트 CPU)가 계산한 기대값과 **4개 원소 전부 일치 (PASS)**
- **Coral NPU(RISC-V)가 신경망 한 층의 추론을 실제 수행** → 데모 업그레이드 완료

---

## 2. `mul`(M-확장 곱셈) 이상 동작 조사 → **정상으로 정정** ⚠️→✅

> 이 항목은 오전에 "`mul` 미지원 아닌가?" 로 의심했다가, 오후에 통제 실험으로 **정상임을 확인**한 과정을 하나로 정리한 것이다. **최종 결론은 "M-확장·곱셈기 정상 동작".**

### (1) 최초 관찰 — `mul` 버전 FC가 멈춤
- FC 1차 버전(`mul` 명령 사용): 보드에서 헤더 출력 후 **멈춤(`done` 미설정)**
- 어제 성공한 `×3` 은 `shift+add` 였고 `mul` 미사용 → 정상이었음
- 임시 대응: 곱셈을 **`shift+add` 소프트웨어 루프**로 구현(`mul` 제거) → 정상 동작
- 이 시점의 가설: "축소 합성된 CoreMiniAxi 구성에서 HW 곱셈기 부재 → SW 우회 필요" (README 리스크와 방향 일치하는 것으로 보였음)

### (2) 통제 실험 — 재검증
- NPU에 **트랩 핸들러(`mtvec`)** 를 걸고 `mul` 단독 실행 → **예외 없이 정상, `6 × 7 = 42`, `done = 1`**
- `mul` 버전 FC(반복문 내 16회 곱셈) 재실행 → `Y = [16, 0, 10, 113]` **정상**

### (3) 정정된 결론
- **M-확장·곱셈기는 정상 동작한다.**
- 앞선 `mul` 버전 멈춤은 하드웨어 한계가 아니라 **일시적 실행 상태**(직전 디버그 세션 미종료로 추정)였다.
- 곱셈은 DSP가 아닌 **LUT로 구현**(면적·속도 특성 차이)일 뿐, **기능은 정상**.
- ➡️ **이후 모델은 `mul` 을 그대로 사용 가능.**

> 📌 발표/문서 반영 시: "M6 한계 = HW 곱셈기 부재" 로 쓰지 말 것. 실제 한계는 *곱셈이 LUT 구현이라 면적·속도 특성이 불리하다* 이며, 기능적 곱셈 지원은 정상이다.

---

## 3. 모델 확장 — 2층 신경망(2→2→1, ReLU)으로 XOR 해결

### 의미
단일 FC층에서 **은닉층이 있는 2층 신경망**으로 확장. "은닉층이 왜 필요한가"의 고전 예제인 **XOR** 를 선택.

### 구성
```
W1 = [[1, 1],
      [1, 1]]      b1 = [0, -1]
W2 = [1, -2]       b2 = 0

hidden = ReLU(W1 · X + b1)
out    = W2 · hidden + b2
```

### 검증 ✅
- 직접 만든 어셈블러 + unicorn 에뮬레이터로 **4입력 전부 통과** (`mul` 사용, 정상 확인됨)
- 보드 실행: `XOR(0,0)=0`, `(0,1)=1`, `(1,0)=1`, `(1,1)=0` **전부 정확**
- **NPU가 2층 신경망 추론 수행**

### 코드
- `03_sw/main_xor.c`

---

## 4. C → RISC-V(rv32im) 컴파일 파이프라인 구축 + 3층 MLP 준비

### 의미
손 어셈블의 한계를 넘어 **NPU 프로그램을 C로 작성해 컴파일**하는 경로를 확보. → **모델 크기 제약이 사라짐.**

### 툴체인
- `sudo` 불가 환경이라 `.deb` 를 받아 **홈 디렉터리에 풀어서 사용**
- `gcc-riscv64-unknown-elf 10.2.0` + `binutils-riscv64-unknown-elf 2.35.1`

### 삽질 기록 (함정 3가지)
1. **gcc가 시스템 x86 `as` 를 호출** → `invalid -march=rv32im`
   → 해결: `-S` 로 **어셈블리만 생성**하고 riscv `as` 로 직접 어셈블
2. **`ld` 기본이 64비트** → `-m elf32lriscv` 명시 필요
3. **`.bss` 가 DTCM `0x10000` 에 잡혀 입력 데이터와 충돌**
   → 링커 스크립트에서 **스크래치 영역(`0x14000`)으로 분리**

### 빌드 흐름 — `03_sw/npu_src/build.sh`
```
gcc -march=rv32im -S            # C → 어셈블리 (컴파일만)
as                              # riscv as 로 직접 어셈블
ld -T link.ld -m elf32lriscv    # 32비트 RISC-V 링크
objcopy (.text)                 # 텍스트 섹션 추출
→ 기계어 워드
```

### 모델 — 3층 MLP `4 → 8 → 8 → 3`, ReLU, argmax까지 C로 작성
- `.text` **328바이트** (ITCM 8KB 대비 충분한 여유)
- `mul` **3개** 사용 (정상 확인됨)
- unicorn 에뮬레이터 **4케이스 전부 기대값 일치 (PASS)**

### 코드
- `03_sw/main_mlp.c` (로더 + 가중치)
- `03_sw/npu_src/{mlp.c, link.ld, build.sh}`

---

## 오늘의 결과 요약

| 항목 | 상태 |
|---|---|
| FC층 추론 `Y=ReLU(W·X+b)` 보드 실행 | ✅ PASS (`X=[3,1,2,5]→Y=[16,0,10,113]`) |
| `mul`(M-확장) 곱셈기 | ✅ **정상** (초기 오판 정정) |
| 2층 신경망 XOR 보드 실행 | ✅ 4케이스 전부 정확 |
| C→rv32im 컴파일 파이프라인 | ✅ 구축 완료 |
| 3층 MLP `4→8→8→3` | ✅ 에뮬레이터 검증 (보드 검증 대기) |

## 다음 할 일
1. FC / MLP 버전으로 **`BOOT.bin` 재생성 → SD 데모 갱신**
2. 보드에서 **3층 MLP 실행 검증**
3. **M6 마무리**: 실행 사이클 · 리소스 사용량 측정, 곱셈 LUT 구현 특성 등 한계 정리
4. 이후 **더 큰 모델(3~4층, 다중 분류) / 실제 학습된 가중치**로 확장

### 2026-07-28 (화) — MNIST 손글씨 인식: 학습된 모델 + int8 양자화로 NPU 추론 🎯
[의미] 임의 가중치 데모를 넘어, 실제 학습된 신경망을 양자화해 NPU에서 추론. 프로젝트 최종 데모.

[모델 학습] scikit-learn digits (8x8 손글씨, 0~9)
- 구조 64 -> 32(ReLU) -> 10, 파라미터 2,410개
- 학습 1,347 / 테스트 450 샘플, float32 테스트 정확도 **98.00%**

[int8 양자화] 대칭 양자화 (scale = max|r|/127)
- 가중치/입력 int8, 누적 int32, 층간 재양자화는 고정소수점 배율로 처리
  · M1 = (s_x*s_w1)/s_h ≈ mult1=1792, shift1=20 → `(a*1792)>>20`
- **int8 정확도 98.00% — float32와 동일, 정확도 손실 0**
- 가중치 메모리 9,640B(float32) → 2,368B(int8), **4배 절감**

[구현·검증]
- 추론 C 코드(mnist.c) → riscv gcc rv32im 빌드, text 272바이트
- unicorn 에뮬레이터로 테스트셋 450개 전량 실행 → 98.00%(441/450), 파이썬 결과와 완전 일치

[보드 실행] 0~9 각 한 장씩 10개 데모
- UART에 8x8 손글씨를 아스키 아트로 출력 + NPU 예측
- **10/10 정확** → 실물 보드에서 손글씨 인식 성공

[코드] 03_sw/main_mnist.c (로더+가중치), 03_sw/npu_src/{mnist.c, train.py, quant.py}
[다음] BOOT.bin 갱신(SD 데모), 성능 측정 보강, 발표자료에 MNIST 결과 반영

### 2026-07-28 (화) — MNIST BOOT.bin SD 독립 부팅 성공 ✅
[내용] MNIST 버전 앱으로 BOOT.bin 재생성(fsbl + bitstream + coral_app.elf, 약 26.7MB) → SD 부팅 확인.
[삽질] SD 작업 중 보드 전원/USB 재연결로 COM 포트가 사라져 PuTTY 접속 실패 → USB 재연결 및 전원 ON 후 정상 인식.
[검증] BOOT.bin 내부에 FSBL(Jul 28 빌드)과 앱 문자열("MNIST handwritten digit recognition on Coral NPU") 포함 확인.
[결과] 컴퓨터·JTAG 없이 SD 부팅만으로 손글씨 인식 데모 자동 실행 → 최종 데모 완성.

### 2026-07-28 (화) — CNN(합성곱 신경망)까지 NPU에서 실행 🧠
[의미] FC(완전연결)만이 아니라 이미지 인식의 표준 구조인 합성곱·풀링까지 NPU에서 동작.

[모델] Conv(8ch 3x3) -> ReLU -> MaxPool(2x2) -> FC(10), 8x8 손글씨
- 직접 구현한 역전파로 학습(numpy), float32 테스트 정확도 96.89%
- 파라미터 810개 (FC 모델 2,410개의 1/3 — 합성곱의 가중치 공유 효과)

[양자화] 동일 방식(대칭 int8, 재양자화 (a*2159)>>20)
- **int8 96.89% — 손실 0**, 가중치 3,240B -> 864B

[핵심 문제와 해결 — 스택 초기화]
- CNN은 지역변수가 많아 **스택을 사용**. 그런데 sp 초기값이 0이라 sp-64가 0xFFFFFFC0이 되어
  잘못된 주소 접근으로 코어 정지.
- 에뮬레이터에서 먼저 재현·발견 → 시작 코드(start.S)에서 sp를 스크래치 상단(0x17000)으로 초기화해 해결.
- 시사점: 단순 프로그램(FC)은 스택 없이 동작했으나, 복잡한 프로그램은 스택 설정이 필수.

[성능 비교]
| 모델 | 파라미터 | 가중치(int8) | 정확도 | 추론 1회 명령어 | MAC당 |
|---|---|---|---|---|---|
| FC 64-32-10 | 2,410 | 2,368B | 98.00% | 17,230 | 7.28 |
| CNN 8ch     |   810 |   864B | 96.89% | 34,920 | 10.54 |
- CNN은 파라미터 1/3이지만 연산량 2배 — 가중치 공유로 메모리는 절약, 대신 같은 커널을 여러 위치에
  반복 적용해 계산량 증가. 메모리/연산 트레이드오프를 실측으로 확인.

[검증] 에뮬레이터 450샘플 전량 96.89%(파이썬과 일치), 보드에서 0~9 데모 **10/10 정확**
[코드] 03_sw/main_cnn.c, 03_sw/npu_src/{cnn.c, start.S, cnn_train.py, cnn_quant.py}

### 2026-07-28 (화) — 외부 LeNet-5(IREE) ELF 탑재 검증: 하드웨어 한계 정량화
[배경] Coral 에뮬레이터에서 동작하던 lenet5_coral.elf를 실제 보드에 올릴 수 있는지 검증 요청.

[정적 분석]
- ELF32 RISC-V, statically linked, not stripped (1.0 MB 파일)
- .text 533,728 B / .rodata 98 KB / .bss 1 MB, 스택 0x200000(2MB) 지점
- 심볼 분석 결과 **IREE 런타임**(VM 바이트코드 인터프리터) 포함
  · iree_vm_bytecode_dispatch(34KB), iree_vm_bytecode_disassemble_op_impl(87KB) 등
  · printf/strtod/pow 등 표준 라이브러리 43개 심볼
  · 모델 데이터는 file_0(56KB), file_1(3KB) flatbuffer
- **벡터(RVV) 명령 0개** → ISA 자체는 우리 빌드와 호환

[요구 vs 실제]
| 항목 | 필요 | 우리 HW | 배수 |
|---|---|---|---|
| .text | 533,728 B | ITCM 8,192 B | 65배 초과 |
| 전체 | 1,684,320 B | 40,960 B | 41배 초과 |
| 스택 | 0x200000 | DTCM 0x10000~0x18000 | 범위 밖 |

[보드 실측] 03_sw/main_lenet_test.c
- ELF .text 일부를 ITCM에 적재 → **write/read 검증 통과**(AXI 경로 정상)
- 그러나 ITCM 용량상 전체 코드의 **1.5%만 적재 가능** → 실행 불가 확인

[결론]
- 크기의 대부분은 모델이 아니라 **IREE 런타임**이다. 에뮬레이터는 메모리 제약이 없어 동작하지만,
  실제 FPGA의 TCM(KB 단위)에서는 근본적으로 수용 불가.
- 대안: (a) NPU를 highmem(ITCM/DTCM 1MB)으로 재합성 — FPGA 리소스 재검토 필요,
        (b) 런타임 없이 네이티브로 재구현 — 우리 방식(같은 과제를 440 B로 수행).
- **시사점**: 에뮬레이션 성공 ≠ 하드웨어 동작. 임베디드 AI에서 런타임 오버헤드가 모델보다 큰 경우가 있다.

### 2026-07-28 (화) — 경량 구조 탐색: Stride-2 Conv 선정
[목적] FC의 낮은 연산량과 CNN의 적은 파라미터를 함께 얻는 구조 탐색.

[방법] 8가지 구조를 동일 조건(시드 3개 평균)으로 학습·비교 — 05_results/model_comparison.md

[결과]
| 구조 | 정확도 | 파라미터 | MAC |
|---|---|---|---|
| FC 64-32-10 | 98.00% | 2,410 | 2,368 |
| CNN 8ch +pool | 97.48% | 810 | 3,312 |
| **Stride2 8ch (채택)** | 96.52% | 810 | **1,368** |
| DSConv 8ch | 94.22% | 755 | 3,636 |

[발견 1] **DSConv(MobileNet 기법)는 이 조건에서 부적합** — 입력이 흑백 1채널이라
depthwise 커널이 하나뿐이어서 표현력 부족(94.2%). 유명 기법도 입력 조건에 따라 달라짐을 실측.

[발견 2] **Stride-2가 최적** — 풀링을 stride로 대체, 연산량 CNN의 41%·FC의 58%,
파라미터는 CNN과 동일. 메모리는 CNN급, 연산은 최저.

[구현] int8 양자화 96.67%→96.44%(-0.2%p), 프로그램 440바이트,
에뮬레이터 450샘플 검증 통과(12,329 명령어/추론). 코드: 03_sw/main_light.c
[상태] 보드 실행 대기 (highmem 재설계 작업을 우선 진행하기로 함)

### 2026-08-12 (수) — 28×28 실제 MNIST를 NPU에서 실행 🎯 + 리눅스 포팅 한계 확인

[배경] 개발 PC SSD 고장 → 임시 PC. 리눅스+SSH+CI 트랙을 시도했으나 버전 호환
문제로 막혀, 검증된 베어메탈 경로로 전환해 28×28을 완성했다.

[모델] FC 784 → 32(ReLU) → 10, 대칭 int8 (기존 8×8은 sklearn digits, 이번은 진짜 MNIST)
- float32 96.36% → **int8 96.41%** (테스트 10,000장, 양자화 손실 없음)
- 학습·양자화는 numpy 직접 구현 (train28.py)

[핵심 문제와 해결 — DTCM 32KB에 25,088개 가중치 수납]
- 8×8 방식(int8 하나를 int32 워드에 저장)이면 100KB → **DTCM 32KB 초과, 불가**
- **바이트 패킹**: int8 4개를 워드 하나에 → 25.4KB로 수납
- NPU 커널은 `lw` + 시프트/마스크로 언팩. `lb`를 안 쓴 이유는 TCM의
  바이트 레인 동작에 의존하지 않기 위해서 (이식성)
- 누적값 × M1이 32bit를 넘으므로 재양자화만 64bit 곱으로 처리
- 커널 크기 1,328B (ITCM 8KB 여유)

[검증 — 3중 일치]
| 단계 | 결과 |
|---|---|
| numpy 정수 시뮬 | 9,641 / 10,000 |
| unicorn 에뮬레이터(rv32) | 60장 전량, **로짓까지 비트 단위 일치** |
| **실제 Coral NPU (보드)** | 데모 10장 예측이 에뮬레이터와 **완전 일치** |

보드 데모 결과 **9/10**. 틀린 5번은 에뮬레이터도 6으로 예측한 샘플로,
하드웨어 오류가 아니라 모델 자체의 오분류 (실제 이미지가 6에 가까운 형태).
→ 시뮬 → 에뮬 → 실물이 어긋나지 않음을 확인.

[리눅스 포팅 시도와 한계 — 기록용]
SSH·GitHub CI 원격 추론을 위해 보드에 리눅스를 올리려 했으나 3단계에서 막힘:
1. 2019.1 프리빌트 FSBL은 psu_init이 우리 설계와 달라 HPM0가 안 열림
   → 0x5_0000_0000 접근 시 DECERR → **SError → 커널 패닉**
   (SError는 비동기 예외라 유저공간 SIGBUS 핸들러로 못 잡는다)
2. 우리 FSBL + 2019.1 ATF/U-Boot로 BOOT.BIN 재조립 → FSBL 배너 후 정지
   (2026.1 FSBL ↔ 2019.1 ATF 조합 불일치)
3. PMU 펌웨어 제외해도 동일
→ **결론**: 제대로 하려면 PetaLinux로 2026.1 전체를 재빌드해야 한다.
   데모 일정상 베어메탈로 선회하는 것이 합리적. 확장 과제로 남김.

[코드] 09_remote/vitis_src/{main_mnist28.c, weights28.h, prog28.h, demo28.h, expected28.h}
       09_remote/sw/train28.py (학습·양자화), 09_remote/npu/mnist28.c (NPU 커널)
[다음] 보드 출력 캡처 → 발표자료 반영, CNN 28×28 확장(선택)

---

# 2026-08-13 — 리눅스에서 Coral NPU 추론 성공 + 완전체 코어 자원 판정

두 갈래를 하루에 처리했다. 하나는 "완전한 Coral NPU를 올릴 수 있는가"에 대한
확정적인 답, 다른 하나는 8/12에 3전 3패했던 리눅스 트랙의 완주다.

## A. 완전체 Coral NPU — ZU9EG에 들어가지 않음 (확정)

Chisel/Bazel로 전체 코어 Verilog를 생성해 실제로 합성했다.

| 타깃 | 구성 | 결과 |
|---|---|---|
| `CoreMiniAxi` | 스칼라 + FP | 현재 보드에서 동작 중 (51,105 LUT) |
| `RvvCoreMiniAxi` | + 벡터/SIMD 128bit | 생성 완료 |
| `VmeCoreMiniAxi` | + 행렬(Zvt) = 완전체 | 생성·합성 완료, **배치 불가** |

### 실측 자원 (VmeCoreMiniAxi, xczu9eg 합성 결과)

| 자원 | 요구 | ZU9EG 보유 | 비율 |
|---|---|---|---|
| **LUT** | **1,002,425** | 274,080 | **366%** |
| FF | 130,494 | 548,160 | 24% |
| BRAM | 10 | 912 | 1% |
| DSP | 281 | 2,520 | 11% |

**합성은 성공했고 배치(place_design)에서 DRC UTLZ-1로 중단.** 회로 설계 문제가
아니라 순수하게 칩 용량 문제다.

주목할 대비는 **DSP 11% vs LUT 366%** 다. 벡터·행렬 유닛의 비용은 곱셈이 아니라
"어느 데이터를 어디로 보낼지 고르는 회로"(먹서·크로스바·마스크 처리)에서 나오고,
FPGA는 정확히 거기에 약하다. `vrgather` 같은 임의 재배열은 16×16 크로스바를
요구하고, 벡터 레지스터 파일은 포트 수 때문에 BRAM에 넣을 수 없어 LUT/FF로 간다.
ASIC이면 트랜지스터 몇 개인 것이 FPGA에서는 64칸짜리 정답표 하나씩을 소모한다.

참고: ZCU104(XCZU7EV)는 LUT 230,400으로 **더 작다.** 보드 교체로 풀 문제가 아니며,
100만 LUT급은 VU9P/VU13P 등 데이터센터용 FPGA 영역이다.

### 합성에 필요한 Verilog define (RTL을 직접 뜯어 확인)

| define | 없으면 |
|---|---|
| `SYNTHESIS` | Sram이 시뮬 경로를 타서 합성 불가 |
| `FPGA_XILINX` | ClockGate가 래치 기반. 있으면 BUFGCE(ULTRASCALE_PLUS) 사용 |
| `ZVE32F_ON` | 벡터 부동소수점 비활성 |
| `ZVT_ON` | 행렬(VME) 확장 비활성 |
| **`VLEN_128`** | **`VLEN` 매크로 미정의 → 561곳에서 구문 오류** |

전처리기 시뮬레이션으로 전수 검사한 결과 `VLEN_128` 하나로 미정의 매크로
617건 → 0건이 됐다(잔여 56건은 전부 주석 안의 백틱).

### 부수적으로 잡은 잠복 버그 (복구 스크립트)

`rebuild_project.tcl`에 실행해 본 적 없는 버그 두 개가 있었다.

1. **HPM1 미사용** — ZynqMP에서 `M_AXI_HPM0_FPD`의 주소창은
   0xA000_0000 / 0x4_0000_0000 / 0x10_0000_0000 이고, 우리가 쓰는
   **0x5_0000_0000은 HPM1_FPD의 창**이다. HPM0만 켜면 주소 배정 자체가 실패한다.
   실제 동작 중인 설계(`coralnpu.bd`)도 HPM0+HPM1을 함께 쓰고 있었다.
2. **마스터 세그먼트까지 강제 이동** — wrapper의 모든 세그먼트를 순회하며
   0x5_0000_0000으로 밀어 m_axi 쪽이 겹쳐 `validate_bd_design` 실패.
   PS→Coral 슬레이브 창 하나만 옮겨야 한다.

PC가 또 고장나 복구가 필요했다면 그대로 실패했을 버그다.

## B. 리눅스 + SSH — 성공

8/12에 세 번 막혔던 원인은 **버전 불일치**였다. PetaLinux 2026.1로 FSBL·PMU·ATF·
u-boot·커널을 **우리 XSA 하나에서 통째로 생성**하니 그 문제가 구조적으로 사라졌다.

부팅과 SSH는 바로 됐지만, NPU 주소에 접근하는 순간 보드가 완전히 멈췄다.
원인이 두 개 더 있었다.

| # | 증상 | 원인 | 대응 |
|---|---|---|---|
| 1 | NPU 접근 시 보드 정지 | 커널이 미사용 클럭 자동 차단(`clk_disable_unused`) | `clk_ignore_unused` |
| 2 | 여전히 SError | 커널이 미사용 **전원 도메인** 자동 차단 | `pd_ignore_unused` |
| 3 | `coral_probe`만 패닉 | CSR `+0x30008`(status) 읽기가 응답 없음 | 로더는 CSR `+0x0/+0x4`만 쓰므로 무관 |

베어메탈에서 같은 코드가 잘 돌았던 이유는 그쪽에 클럭·전원 자동 차단 절차가
없기 때문이다.

### 진단 방법 — 읽기 전용 탐침

쓰기 실패는 ARM에서 **비동기(SError)** 로 통보돼 커널 패닉이 되고 프로그램이
잡을 수 없다. 반면 **읽기 실패는 동기(SIGBUS)** 라 잡을 수 있다.
그래서 mmap 기반 읽기 전용 탐침(`09_remote/sw/rdprobe.c`)을 만들어
보드를 죽이지 않고 주소 지도를 실측했다.

```
0x5_0000_0000 + 0x0000   READ OK      ITCM 시작
0x5_0000_0000 + 0x2000   BUS ERROR    ITCM 8KB 끝     <- 사양과 일치
0x5_0000_0000 + 0x10000  READ OK      DTCM 시작
0x5_0000_0000 + 0x18000  BUS ERROR    DTCM 32KB 끝    <- 사양과 일치
```

경계가 8KB/32KB에서 정확히 떨어진 것이 PL에 올라간 것이 우리 Coral 코어이고
정상 응답 중이라는 결정적 증거였다.

이어서 쓰기 경로를 모드별로 분리해 시험(`09_remote/sw/wrtest.c`)한 결과
DTCM 32bit / ITCM 32bit / DTCM 128bit 쓰기가 모두 정상이었다.

### 결과 — 검증 경로 네 개가 모두 일치

| 경로 | 실행 위치 | 결과 |
|---|---|---|
| numpy 정수 시뮬 | PC | 9,641 / 10,000 |
| unicorn 에뮬레이터 | PC | 로짓까지 비트 일치 |
| 베어메탈 (07-28) | 실제 보드 | 9 / 10, 오답 5→6 |
| **리눅스 (08-13)** | **실제 보드** | **9 / 10, 오답 5→6** |

오답의 내용까지 같다. 배선·타이밍·양자화 중 하나라도 어긋났다면 틀리는
방식이 달랐을 것이다.

캡처: `05_results/uart_capture/LINUX_NPU_20260813.md` 및 같은 폴더의 PNG 4장

## 환경 정비 (임시 PC)

- Vivado 라이선스: 리호스팅한 .lic이 설치돼 있지 않았음. HOSTID를 유선 랜카드
  MAC으로 맞춘 파일로 교체 → **Enterprise, 2026-10-10까지 유효** (8/23 만료는 오해)
- WSL2 + Ubuntu 22.04 신규 설치 → Bazel/Chisel 및 PetaLinux 2026.1 환경 확보
- Coral 빌드는 **clang 필수** (`.bazelrc`가 `--action_env=CC="clang"`을 강제).
  gcc만 있으면 툴체인 설정 단계에서 즉시 실패한다.

## 남은 것

1. 부팅 옵션(`clk_ignore_unused pd_ignore_unused`)을 이미지에 영구 반영
   — 현재는 매 부팅 u-boot에서 수동 입력. `image.ub`가 재생성되지 않는 문제 확인 필요
2. 고정 IP / root 비밀번호 이미지 반영 (사무실 DHCP가 미등록 MAC에 IP를 주지 않음)
3. GitHub CI 러너 연결 → `git push` 시 실제 보드 추론 자동화
4. 10,000장 전수 평가로 9,641 재현 확인

## 발표 정리 방향

- **하드웨어** — Coral NPU 스칼라 구성(CoreMiniAxi)을 FPGA에 구현, LUT 18.6% 사용
- **시스템** — ARM 측에 PetaLinux 2026.1을 올려 SSH 기반 원격 추론 환경 구축
- **한계 분석** — 완전 구성은 LUT 1,002,425개로 ZU9EG의 3.7배. 합성으로 실측 확인

"완전체를 못 올렸다"가 아니라 "어디까지 되고 왜 거기서 멈추는지 숫자로 밝혔다"
가 정확한 서술이다.



## 2026-08-14: SD ext4 전환 완료 ★

initramfs(램) rootfs → SD ext4 rootfs 전환을 **완료**. 재부팅 후 설정 영구 유지 확인.

### 한 작업
- SD를 두 파티션으로 재구성: p1 FAT32 1GB(BOOT.BIN, image.ub, boot.scr, board/), p2 ext4(rootfs).
- 카드리더가 다중 슬롯이라 `wsl --mount --bare`가 0x8007000f로 실패 → **usbipd로 우회**
  (`usbipd bind/attach --wsl --busid 3-1`, 카드리더 VID 05e3:0748).
- WSL에서 `/dev/sdg2` mkfs.ext4 → 진짜 rootfs.tar.gz(47MB) 풀기.
- 첫 부팅 시 **emergency mode** (모든 마운트 실패, `mount: must be superuser`).
- 원인: `id` → **euid=1000**, rootfs 전체가 uid/gid 1000 소유. tarball에 root 소유가 안 담김.
- 해결: WSL에서 `sudo chown -R 0:0 /mnt/sdroot` → 정상 부팅 → login → passwd → 재부팅 후 유지 확인.

### ★ 새 함정 3-9 — rootfs tarball은 root 소유로 만들어야 한다
Yocto rootfs는 pseudo 안에서만 root:root 로 보인다. 그냥 `tar`로 묶으면 빌드계정(1000) 소유가
담겨, SD에 풀면 euid=1000이 되어 mount 전부 실패 → emergency mode.
**해결책(영구): `make_rootfs.sh`의 tar 명령에 `--owner=0 --group=0` 추가**
(`tar czf rootfs.tar.gz --numeric-owner --owner=0 --group=0 -C <rootfs> .`).
그러면 아카이브 내 모든 파일이 root 소유로 강제되어 재발 안 함.
임시 대응은 푼 뒤 `chown -R 0:0`.

### 현재 상태
- 보드: SD ext4 부팅 정상, root/root 로그인, 설정 영구 저장.
- 안전망: `D:\backup_sd_ext4`(SD 굽는 재료 47MB rootfs 포함), PetaLinux 빌드트리(`~/coral_plnx/coral_zcu102`) 정상.

### 다음 순서
1. (진행) CI 통과 확인 — ext4 전환 후 board 경로 변동 확인. 예전 수동 성공 경로는
   `/run/media/mmcblk0p1/board/run_demo.sh`. ext4에선 마운트 위치가 바뀔 수 있음(부팅 로그에
   `/run/media/BOOT-mmcblk0p1` 마운트 실패 흔적 있었음 — 확인 필요).
2. 러너 `C:\actions-runner`로 이전.
3. 발표자료(pptx).
4. (권장) make_rootfs.sh에 `--owner=0 --group=0` 반영해 3-9 재발 방지.
