### 2026-07-20 (월) — Coral NPU 전환 및 빌드 환경 구축

[방향 전환]
- 교수님 지시로 대상 NPU를 NVDLA → **Google Coral NPU** 로 변경
- 목표: ZCU102에서 input → 추론 → output 동작 (베어메탈)
- 선배 확인: 리눅스 서버 불필요, 윈도우 Vivado 2026.1로 진행

[Coral 레포 조사]
- fpga/README.md 확인 → **Google 사내 보드("Nexus") 전용 문서**
  · nexusXX.mtv.corp.google.com 접속, nexus_loader / zturn 등 비공개 사내 도구 사용
  · ZCU102를 포함해 공개 보드 지원은 없음 → AXI/XDC 직접 작성 필요
- hdl/verilog/ 에는 Sram.v, ClockGate.sv, RstSync.sv 등 부품 셀만 존재
  · NPU 본체는 hdl/chisel (Scala) → **Bazel로 Verilog 생성 단계 필요**
- fpga/coralnpu_soc.core 분석
  · vivado 합성 타깃 존재, FPGA_XILINX / USE_GENERIC 파라미터 있음 (긍정)
  · 단, part = "xcvu13p-fhga2104-2-e" (Virtex UltraScale+, 약 3,780K LC)
  · ZCU102(ZU9EG)는 약 600K → **6배 이상 작음.** SoC 전체 합성은 불가 예상
  · 대응: NPU 코어만 분리 합성 → 축소 범위 자체를 분석 결과로 삼음

[환경 구축 — 완료]
- WSL2 + Ubuntu 22.04 설치 (윈도우 유지, 듀얼부팅 아님)
  · 초기 OOBE가 멈춰 wsl --unregister 후 재설치하여 해결
- build-essential / git / python3(3.10) / srecord / curl / zip / unzip 설치
- bazelisk 설치 → /usr/local/bin/bazel
- coralnpu 레포 clone (리눅스 홈 ~/coralnpu, 윈도우 경로는 빌드 속도 문제로 회피)
- .bazelversion = **8.6.0** (README의 7.4.1과 상이하나 bazelisk가 자동 처리)

[진행 중]
- bazel build //examples:coralnpu_v2_hello_world_add_floats (첫 빌드, 장시간 소요)

[다음]
- M1: 예제 빌드 성공 확인
- M2: Verilator 시뮬에서 Coral 실행 + mcycle 카운터 측정
- M3: NPU 코어 분리 → ZCU102 타깃 합성, 리소스 리포트 확보
