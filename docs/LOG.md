# 실험 일지

> **규칙: 삽질한 것도 반드시 적는다.**
> 8/31에 교수님이 가장 관심 있게 볼 파일이 이거다.
> "무엇이 안 됐고, 왜 안 됐고, 어떻게 알아냈는가"가 결과물이다.
> 매일 5분. 밀리면 기억 안 나서 못 쓴다.

---

### 2026-07-14 (화) — 프로젝트 시작 (NVDLA 단계)

- 목표: 개발 환경 파악, ITRI-OpenDLA 확보, 보드 첫 부팅
- 한 일:
  · GitHub 레포 생성 및 프로젝트 뼈대 커밋
  · ITRI-OpenDLA 다운로드 → `nvsmall64_zcu102/a38/project_1.sdk/SD_BOOT/bootimage/`
    에서 사전빌드 **BOOT.bin (17.8MB)** 발견
    → 비트스트림이 통째로 포함된 크기. 베어메탈 구조로 추정되어
      PetaLinux/리눅스 서버 없이도 진행 가능하다고 판단
  · CP210x 드라이버 설치 (CP2108 Interface 0~3 → COM6/3/4/5)
    ⚠️ UART는 **Interface 0** 사용. COM 번호가 가장 낮은 것이 아님(COM6)
  · SD카드(FAT32)에 BOOT.bin 복사, SW6 = 1-ON / 2,3,4-OFF, PuTTY 115200
- 결과: **FSBL 배너 3줄 출력 후 정지**
  ```
  Xilinx Zynq MP First Stage Boot Loader
  Release 2018.3
  PMU-FW is not running, certain applications may not be supported.
  ```
- 원인 추정: ITRI BOOT.bin의 2018.3 FSBL ↔ 보드 Rev 1.1 불일치
- 다음: Xilinx 공식 프리빌트 이미지로 진단 확정

---

### 2026-07-15 (수) — 원인 확정 + 부팅 성공 (NVDLA 단계)

[보드 진단]
- AMD 공식 ZCU102 **2019.1 프리빌트 이미지**(BOOT.BIN + image.ub)로 부팅 테스트
- 결과: **로그인 프롬프트까지 정상 부팅 성공** ✅
  ```
  PetaLinux 2019.1 xilinx-zcu102-2019_1 /dev/ttyPS0
  xilinx-zcu102-2019_1 login: root
  ```
- 하드웨어 확인: 커널 4.19 aarch64 / nproc=4 / FPGA state=operating (전부 정상)
- **원인 확정: 어제 멈춤 = ITRI의 2018.3 FSBL ↔ 보드 Rev 1.1 불일치**
  → 보드·SD·UART·스위치는 모두 정상. 순수 소프트웨어(FSBL 버전) 문제

[네트워크 / SSH]
- 보드에 IP 수동 할당 (10.126.37.57)
- SSH 접속 성공. 단, 옵션 필요:
  `ssh -o HostKeyAlgorithms=+ssh-rsa root@10.126.37.57`
  → 보드의 Dropbear(2019년)는 ssh-rsa만 지원, 최신 OpenSSH가 거부하기 때문
- **VS Code Remote-SSH는 불가.** 보드 리눅스가 BusyBox라
  `tar --no-same-owner`, `wget --no-config` 미지원 → VS Code 서버 설치 실패
  → 임베디드 보드에는 구조적으로 붙지 않음. 순수 SSH 터미널은 정상 동작

[이론 학습]
- 부팅 5단계 릴레이 이해:
  BootROM → FSBL → PMU/ATF → U-Boot → Linux
- BOOT.bin = FSBL + PMU + ATF + 비트스트림 + U-Boot 를 묶은 파일

[.bif 분석 — 핵심 발견]
- ITRI의 `SD_BOOT.bif` 내용:
  ```
  the_ROM_image:
  {
      [fsbl_config]a53_x64
      [bootloader]...\SD_BOOT.elf
      [destination_device = pl]...\zcu102_base_trd_wrapper.bit
  }
  ```
- **`[pmufw_image]` 항목이 아예 없음** → PMU 펌웨어 누락 확인
  → "PMU-FW is not running" + Rev 1.1 부팅 실패의 직접 원인
- 해결 방향: 비트스트림(.bit)은 유지, FSBL 교체 + PMU 추가 → bootgen 재조립
- `.hdf`(zcu102_base_trd_wrapper.hdf)가 FSBL/PMU 생성의 핵심 재료
- Vivado 2026.1은 `.hdf` 미지원(2019.2부터 .xsa) → 구버전 필요 확인

---

### 2026-07-15 (수) 밤 — Coral NPU로 방향 전환

- **교수님 지시로 대상 NPU 변경: NVDLA → Google Coral NPU**
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


### 2026-07-27 (월) — M5 완성: Coral NPU 실제 실행 + BOOT.bin SD 부팅 성공 🎯✅

[목표] NPU에 프로그램을 적재·실행해 input→추론→output 확인 → BOOT.bin으로 독립 부팅

[NPU 실행 프로그램 — 기계어 하드코딩]
- 툴체인(WSL/Bazel) 없이, 작은 RISC-V 프로그램을 기계어(8워드)로 직접 작성해 ITCM에 적재
  · lui/lw/slli/add/sw/addi/sw/j → output = input*3, done=1 후 무한루프
  · Python으로 각 명령어 인코딩 디코드 검증 후 사용
- 데이터 규약(DTCM): input +0x0, output +0x4, done +0x8

[로더 (A53 베어메탈)]
- 절차: 리셋 유지(CSR+0x0=1) → ITCM에 프로그램 write → DTCM에 input write, done=0
  → CSR+0x4=0(entry) → 실행(CSR+0x0: 0x1 → 0x0) → DTCM done 폴링 → output 읽기
- CSR+0x8(status)는 0으로 관찰됨 (코어가 계산 후 무한루프 상태라 정상)
  → 완료 감지는 DTCM done 플래그 폴링으로 처리 (status 비의존)

[JTAG 실행 결과]
- input 7/10/100/1234 → output 21/30/300/3702 전부 OK
- **Coral NPU(RISC-V 코어)가 명령어를 실제 실행해 계산 → M5 핵심 달성** ✅

[BOOT.bin 생성 + SD 부팅]
- Vitis Create Boot Image: fsbl.elf + coralnpu_wrapper.bit + coral_app.elf → BOOT.bin (약 26MB)
  · PMU 펌웨어는 미포함 (없어도 앱 정상 동작 확인, "PMU-FW not running" 경고는 무해)
- SD카드(FAT32) 루트에 BOOT.bin 복사, 옛 image.ub 삭제
- 부팅 모드 SW6: 1-ON / 2·3·4-OFF (SD)
- 전원 ON → FSBL → 비트스트림 로딩 → coral_app 자동 실행
- **컴퓨터·Vitis·JTAG 없이 보드 전원만으로 NPU 동작 확인** → 데모 완성본 확보 🎯

[확보한 핵심 정보 — 재사용용]
- s_axi 창 맵: ITCM 0x5_0000_0000 / DTCM 0x5_0001_0000 / CSR 0x5_0003_0000
- CSR: +0x0 reset·clock, +0x4 entry PC, +0x8 status
- 클럭 50MHz (WNS +0.462)

[다음 — 확장 (선택)]
1. NPU 데모 업그레이드: scalar(x3) → vector(SIMD)/행렬 연산으로 Coral 강점 시연
2. M6: 실행 사이클 수 · LUT/DSP/BRAM 사용량 측정 및 한계 분석
3. (선택) PMU 펌웨어 포함해 부팅 경고 제거
