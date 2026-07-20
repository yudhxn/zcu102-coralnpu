### 2026-07-20 (월) — M3 달성: Coral NPU ZCU102 합성 성공 ✅

[합성 조건]
- 도구: Vivado 2026.1 (Windows)
- 타깃: xczu9eg-ffvb1156-2-e (ZCU102 Evaluation Board)
- Top module: **CoreMiniAxi**
- 소스: CoreMiniAxi.sv 단일 파일 (36,725줄 / 1.6MB)
  · Chisel → Verilog 생성물 (Bazel `emit_verilog`)
  · ClockGate, RstSync, Sram 모듈이 모두 내장되어 있어 추가 파일 불필요
- Verilog define: **`SYNTHESIS`**
  · 이 define이 SRAM 구현 분기를 결정함
  · 정의 시 → 합성 가능한 동작 기술 메모리 (`bit [127:0] mem[...]`) → BRAM 추론
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
- 사전 우려: coralnpu_soc.core의 합성 타깃이 xcvu13p (약 3,780K LC)로,
  ZCU102(ZU9EG, 약 600K LC)의 6배 규모 → 수용 불가 예상
- 실제 결과: **기우였음.** 해당 타깃은 SoC 전체(UART/I2C/ISP/ROM/버스 포함) 기준이었고,
  NPU 코어(CoreMiniAxi)만 분리하면 LUT 18% 수준
- 결론: **SoC가 아닌 CoreMiniAxi 단독 분리 전략이 유효함이 정량적으로 입증됨**

[Bonded IOB 873.78% — 문제 아님]
- 원인: CoreMiniAxi를 최상위로 단독 합성하여 AXI 신호 2,866개가
  모두 외부 물리 핀(IOB)으로 매핑됨. ZCU102 IOB는 328개
- M4에서 PS(ARM)와 AXI로 내부 연결하면 해당 신호는 칩 내부 배선이 되어 IOB를 소비하지 않음
- 별도 조치 불필요

[관찰 / 후속 분석 대상]
- DSP를 6개만 사용 → 매트릭스 연산이 DSP48E2가 아닌 LUT로 구현된 것으로 보임
  · USE_GENERIC 경로의 영향 가능성. M6 분석 항목으로 기록
- BRAM 10개 ≈ ITCM 8KB + DTCM 32KB (사양과 일치)
- LUT6 25,975개로 연산 로직이 LUT에 집중

[포트 구조 확인 — M4 설계 근거]
- io_aclk / io_aresetn : 단일 클럭, 단일 리셋 (설계 단순)
- io_axi_slave_*  : addr 32b / data **128b** → PS M_AXI_HPM → Coral 제어
- io_axi_master_* : addr 32b / data **128b** → Coral → PS S_AXI_HP (메모리 접근)
- 표준 AXI4 5채널 구조 그대로 → ZCU102 HP 포트(128b)와 폭 일치, 변환 로직 불필요

[다음 — M4]
1. Vivado Block Design 생성 (Zynq UltraScale+ PS + CoreMiniAxi)
2. PS M_AXI_HPM0 → Coral slave 연결
3. Coral master → PS S_AXI_HP0 연결
4. Implementation → bitstream 생성
5. Vitis 베어메탈 앱: input 주입 → output 확인 (UART)
6. BOOT.bin 생성 (NVDLA 단계에서 습득한 bootgen 활용)