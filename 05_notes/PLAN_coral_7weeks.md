# Coral NPU on ZCU102 — 7주 계획 (가안)

> ⚠️ **가안 주의:** 시나리오 A(Coral 레포에 ZCU102 지원이 있다) 전제.
> 서버에서 `platforms` 폴더 확인 후 크게 수정될 수 있음.
> 작성: 2026-07-15 / 마감: 2026-08-31 (교수님 데모)

---

## 목표
NVIDIA NVDLA → **Google Coral NPU**로 방향 전환 (교수님 지시).
ZCU102 보드에 Coral NPU를 **합성(FPGA에 올리기) → 실제 추론**까지.

## NVDLA vs Coral — 지형이 다름

| | NVDLA (폐기) | Coral (신규) |
|---|---|---|
| 언어/빌드 | Verilog | SystemVerilog + Bazel |
| 모델 컴파일 | Caffe → .nvdla | TFLite → MLIR/IREE |
| 검증 도구 | cmodel | Verilator 시뮬 (README에 바로) |
| 강점 | 레퍼런스 풍부 | 시뮬레이터 진입 쉬움 |
| 약점 | 구식 툴체인 | ZCU102 사례 없음 |

**핵심 전략:** Coral은 Verilator 시뮬이 쉬움 → "시뮬 먼저 성공 → FPGA" 순서.
NVDLA와 반대. 이게 안전망이 됨.

---

## 주차별 계획

### W1 (7/16–7/22) — 환경 구축 + 시뮬 첫 성공
- 서버 접속(VPN/계정) + Bazel 7.4.1 / Python 3.9~3.12 / SRecord 설치
- 레포 clone → **platforms 폴더에 ZCU102 있는지 확인 (분기점!)**
- README Quick Start: `bazel run`으로 테스트 통과 + hello_world 시뮬
- ✅ M1: Verilator 시뮬에서 Coral 코드 실행 성공
- 이 주의 진짜 목표: "Bazel이 서버에서 돈다" 확인

### W2 (7/23–7/29) — 아키텍처 이해 + 시뮬로 연산
- 구조 파악: scalar(RISC-V) + vector(SIMD) + matrix(MAC) 3부분
- 예제 시뮬 실행 → 성능 카운터(mcycle) 읽기
- MLIR/IREE 툴체인 맛보기 (TFLite → Coral 바이너리 흐름)
- ✅ M2: 시뮬에서 연산/모델 실행 + 사이클 측정

### W3 (7/30–8/5) — FPGA 합성 (첫 번째 큰 산)
- fpga/platforms의 합성 스크립트 분석
- Vivado로 Coral → 비트스트림 합성
- 리소스 리포트(LUT/BRAM/DSP), ZCU102에 들어가는지 확인
- ⚠️ 최대 병목. ZCU102 미지원 시 인터페이스 설계로 확장
- ✅ M3: Coral 비트스트림 합성 성공 (ZCU102 타깃)

### W4 (8/6–8/12) — 보드에 올리기
- 비트스트림 + PS(ARM) 연결
- BOOT.bin 만들기 ← **지난 2주 삽질 경험 그대로 재사용!**
- 보드 부팅 → Coral 인식 확인
- ✅ M4: ZCU102에서 Coral NPU 부팅 + 인식

### W5 (8/13–8/19) — 실제 추론 (두 번째 큰 산)
- TFLite 모델(MNIST 등 간단한 것) → MLIR/IREE 컴파일
- 보드의 Coral로 추론 실행 + 정확도 검증
- ✅ M5: 보드에서 실제 모델 추론 성공 ← 목표 달성선

### W6 (8/20–8/26) — 측정·분석 ("한계까지")
- CPU vs Coral NPU 속도/전력 비교
- 사이클 카운터 분석, 되는/안 되는 모델 구분, 한계 규명
- ✅ M6: 정량 데이터 + 한계 분석

### W7 (8/27–8/31) — 마감
- 재현 스크립트, 결과 그래프, 발표자료
- 🎯 8/31 교수님 데모

---

## 지난 NVDLA 작업이 재활용되는 부분

| 배운 것 | Coral에서 |
|---|---|
| 보드 부팅 / SW6 / UART | W4 동일 |
| FSBL / PMU / bootgen | W4 BOOT.bin 만들 때 그대로 |
| Rev 1.1 FSBL 문제 | W4에서 미리 회피 |
| SSH / 서버 환경 | W1부터 활용 |
| 부팅 5단계 원리 | 전 과정 이해 기반 |

→ 특히 W4(BOOT.bin)는 이미 절반 아는 상태.

---

## 리스크 3개
1. **Bazel 지옥** — 낯선 빌드 시스템. W1 못 뚫으면 전체 밀림.
2. **ZCU102 미지원** — platforms에 없으면 W3~W4 급난이도. 서버 붙자마자 확인.
3. **MLIR 컴파일러** — TFLite→Coral 변환이 문서대로 안 될 수 있음. W2에서 미리.

## 최소 방어선 (마감 압박 시)
- FPGA 합성(W3) 실패해도 → "Verilator 시뮬에서 추론 + 성능 분석"으로 결과물 가능
- W1~W2(시뮬)만 확실히 잡으면 최악에도 빈손 아님 ← Coral의 안전망

---

## 교수님 확인 필요 사항
1. Coral NPU의 ZCU102 공식 지원 불확실 → 없으면 인터페이스 직접 설계, 난이도 급상승
2. 목표가 "합성+추론"인지, "시뮬 검증까지"도 인정인지
3. 8/31 마감 그대로인지 (난이도 상승 반영 여지)
4. NVDLA 작업물은 비교용으로 남길지
