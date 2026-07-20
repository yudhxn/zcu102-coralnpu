### 2026-07-20 (월) — Coral NPU 전환 및 시뮬레이션 실행 성공 ✅

[방향 전환]
- 교수님 지시로 대상 NPU를 NVDLA → **Google Coral NPU** 로 변경
- 목표: ZCU102에서 input → 추론 → output (베어메탈)
- 선배 확인: 리눅스 서버 불필요, 윈도우 Vivado 2026.1로 진행

[Coral 레포 조사]
- fpga/README.md → **Google 사내 보드("Nexus") 전용 문서**
  · nexusXX.mtv.corp.google.com 접속, nexus_loader / zturn 등 비공개 사내 도구
  · ZCU102 포함 공개 보드 지원 없음 → AXI 연결·XDC 직접 작성 필요
- hdl/verilog/ 에는 Sram.v, ClockGate.sv, RstSync.sv 등 부품 셀만 존재
  · NPU 본체는 hdl/chisel (Scala) → Bazel로 Verilog 생성 단계 필요
- fpga/coralnpu_soc.core 분석
  · vivado 합성 타깃 존재, FPGA_XILINX / USE_GENERIC 파라미터 있음 (긍정)
  · part = "xcvu13p-fhga2104-2-e" (Virtex UltraScale+, 약 3,780K LC)
  · ZCU102(ZU9EG)는 약 600K → **6배 이상 작음.** SoC 전체 합성 불가 예상
  · 대응: NPU 코어만 분리 합성 → 축소 범위 자체를 분석 결과로 삼음

[환경 구축 — 완료]
- WSL2 + Ubuntu 22.04 (윈도우 유지, 듀얼부팅 아님)
  · 초기 OOBE 멈춤 → wsl --unregister 후 재설치로 해결
- build-essential / git / python3(3.10) / srecord / curl / zip / unzip
- bazelisk → /usr/local/bin/bazel
- coralnpu clone (리눅스 홈 ~/coralnpu. 윈도우 경로는 빌드 속도 문제로 회피)
- .bazelversion = 8.6.0 (README의 7.4.1과 상이하나 bazelisk가 자동 처리)

[빌드 & 실행 — 성공]
1. 예제 빌드 (222s)
   bazel build //examples:coralnpu_v2_hello_world_add_floats
   → .elf / .bin / .vmem 생성

2. 시뮬레이터 빌드 (368s)
   bazel build //tests/verilator_sim:core_mini_axi_sim
   → 로그에 `core_mini_axi_cc_library_emit_verilog` 확인
   → **Chisel → Verilog 변환이 실제로 수행됨**
   → Verilator: 124 modules, 1.592 MB sources (M3 합성 재료 확보)

3. 시뮬레이션 실행 성공
   → "Simulation stopped by user" 정상 종료 (core dump 없음)
   → **Coral NPU RTL이 로컬에서 RISC-V 바이너리를 실제 실행함**

[삽질 기록 — 경로 문제]
- examples(RISC-V)와 verilator_sim(x86)이 서로 다른 빌드 설정
  → 하나를 빌드하면 bazel-bin 심볼릭 링크가 그쪽으로 이동,
    다른 하나가 "No such file or directory" 로 사라짐
- 출력 경로가 서로 다름:
  · 시뮬레이터: bazel-out/k8-fastbuild/
  · ELF:        bazel-out/k8-fastbuild-ST-dd8dc713f32d/
- 해결: 양쪽 모두 **절대 경로**로 지정하여 실행. 환경변수 $SIM / $ELF 로 등록

[결과]
- ✅ M1 완료 (빌드 환경 구축 + 예제 빌드)
- ✅ M2 진입 (시뮬레이터에서 Coral 실행)

[다음]
- M2: 사이클 카운터(mcycle) 측정, 다른 예제 실행
- M3: 생성된 Verilog 확보 → NPU 코어 분리 → ZCU102 타깃 Vivado 합성
      → 리소스 리포트로 실제 크기 확인