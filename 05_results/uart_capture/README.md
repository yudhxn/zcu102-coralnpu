# UART 출력 — 실제 보드 캡처 + 재현본

## 2026-08-11 실물 검증 완료

임시 PC + 복구한 `BOOT.bin`으로 보드를 다시 부팅해 **실제 출력을 확보**했습니다.
발표자료에는 `mnist_uart_BOARD_20260811.txt` / `putty_mnist_BOARD_*.png` (실물)을
쓰시고, 재현본은 참고용으로만 두세요.

- 결과: **10 / 10 correct**
- 콘솔: COM3 (Silicon Labs Quad CP2108 Interface 0), 115200 8N1
- FSBL: Release 2026.1, Jul 28 2026 빌드

**재현본과 실제 보드 출력을 대조한 결과 완전히 일치했습니다.**
(FSBL 배너·빈 줄·후행 공백 제외, `diff` 무차이) — 8/10에 만든 호스트 시뮬레이션이
정확했음이 실물로 검증된 셈입니다.

---

## (참고) 재현본에 대하여

보드를 쓸 수 없던 시점에, **보드에 올렸던 것과 동일한 소스·가중치·샘플**로
PC에서 그대로 실행해 UART 출력을 재현한 것입니다.

## 왜 보드 출력과 동일한가
- 출력 문자열: `03_sw/main_mnist.c`, `03_sw/main_cnn.c` 의 `xil_printf` 원문 그대로
- 입력 샘플·가중치: 같은 파일 안에 상수 배열로 박혀 있음 (`samples`, `pics`, `qW1/qb1/qW2/qb2`)
- NPU 연산: `03_sw/npu_src/mnist.c`, `cnn.c` 의 **정수 전용** 연산을 그대로 복사
  (int8 곱셈 → int32 누적 → 고정소수점 재양자화 `(a*M)>>20` → argmax)
- 부동소수점·난수·타이밍 의존 요소가 전혀 없으므로 결과가 결정론적 → 글자 단위로 동일

## 대조 검증 (docs/LOG.md 2026-07-28 기록)
| 항목 | LOG.md 기록 | 재현 결과 | 일치 |
|---|---|---|---|
| MNIST 보드 데모 | 10/10 정확 | 10 / 10 correct | O |
| CNN 보드 데모 | 10/10 정확 | 10 / 10 correct | O |
| MNIST int8 정확도 | 98.00% | 배너 0.980 | O |
| CNN int8 정확도 | 96.89% | 배너 0.969 | O |

## 파일
- `mnist_uart.txt`, `cnn_uart.txt` : 재현된 UART 출력 원문
- `putty_mnist_full.png`, `putty_cnn_full.png` : 전체 로그 PuTTY 화면
- `putty_mnist_summary.png`, `putty_cnn_summary.png` : 발표용 요약본 (0,1,9 + 최종 집계)
- `kern_*.c`, `xil_*.h`, `render.py` : 재현에 쓴 호스트 시뮬 shim과 렌더 스크립트

## 발표 시 주의
실제 보드 화면 사진이 아니라 **동일 코드의 재현 출력**입니다.
질문이 나올 수 있으니 캡션에 "재현 출력 (보드 실행 기록: LOG.md 2026-07-28, 10/10)"
정도로 적어두고, 보드를 다시 쓸 수 있게 되면 실제 화면으로 교체하는 것을 권장합니다.
