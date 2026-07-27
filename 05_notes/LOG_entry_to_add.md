### 2026-07-15 밤 — Coral NPU로 방향 전환

- 교수님 지시: NVDLA → Google Coral NPU로 변경 (ZCU102에 Coral 올리기)
- 목표: 합성(FPGA) → 실제 추론까지

[Coral NPU 팩트체크 결과]
- 레포 활발 (커밋 1403, 별 2.4k), fpga/platforms/hdl 폴더 존재 → FPGA 지원 공식
- 매트릭스 코어 릴리스됨 (M3-2026-04-27) → AI 계산 핵심 확보된 듯
- 빌드 = Bazel 7.4.1, 언어 = SystemVerilog/Scala
- Verilator 시뮬로 바로 실행 가능 (README Quick Start 있음)
- RISC-V 기반 (scalar + vector SIMD + matrix MAC)

[미확인 — 서버에서 확인할 것]
- platforms 폴더에 ZCU102 지원 있는지 → 프로젝트 난이도 좌우
  · 있으면: 스크립트 따라 가능
  · 없으면: AXI 인터페이스 직접 설계 필요 (난이도 급상승)

[판단]
- 불가능하진 않음. 단 NVDLA보다 어렵고 ZCU102 레퍼런스 없음.
- 안전망: Verilator 시뮬 진입이 쉬워서, 최악의 경우 시뮬 검증까지는 확보 가능.

[재활용 가능한 지난 작업]
- 보드 부팅/SW6/UART, FSBL/PMU/bootgen, SSH/서버, 부팅 5단계 원리
- 특히 BOOT.bin 만들기(W4)는 이미 절반 아는 상태

[다음]
- 교수님께: 목표 범위/마감/ZCU102 지원 여부 확인
- 서버 붙으면: 레포 clone → platforms 폴더부터 확인 → 시뮬 첫 실행(M1)
- 새 7주 계획: PLAN_coral_7weeks.md 참조
