# 프로젝트 재건 절차 (SSD 고장 → 임시 PC)

SSD 인식 불가로 Vivado 프로젝트·비트스트림·BOOT.bin이 사라진 상황에서,
GitHub에 남은 RTL 소스와 `docs/LOG.md` 기록만으로 데모 완성본을 복구하는 절차.

**목표는 단 하나 — `BOOT.bin` 확보.** 이것만 있으면 SD카드 부팅으로
컴퓨터·JTAG·Vivado 없이 데모가 돌아간다.

---

## 상황 요약

| 항목 | 상태 |
|---|---|
| RTL 소스 (`.sv`) | GitHub에 있음 |
| 소프트웨어 (`main_*.c`, `npu_src/`) | GitHub에 있음 |
| Vivado 프로젝트 / `.bit` / `.xsa` / `BOOT.bin` | **없음** (`.gitignore`로 제외됐었음) |
| Vivado 라이선스 | 60일 평가판, **8/23경 만료** |
| 데모 | 8/31 |

즉 **라이선스가 살아있는 약 12일 안에 A~C를 끝내야 한다.**
D 이후는 라이선스와 무관하다.

---

## A. Vivado — 비트스트림 + XSA

### A-0. 사전 준비

1. **라이선스 등록 확인**
   `xczu9eg`는 무료 에디션으로 합성이 안 된다. 먼저 등록할 것.

   ```powershell
   New-Item -ItemType Directory -Force -Path $env:USERPROFILE\.Xilinx | Out-Null
   Copy-Item $env:USERPROFILE\Downloads\Xilinx.lic $env:USERPROFILE\.Xilinx\Xilinx.lic -Force
   [Environment]::SetEnvironmentVariable("XILINXD_LICENSE_FILE", "$env:USERPROFILE\.Xilinx\Xilinx.lic", "User")
   ```

2. **ZCU102 보드 파일 설치**
   Vivado → `Tools` → `Vivado Store` → `Boards` → ZCU102 검색 → Install
   (이게 없으면 PS의 DDR4/UART 프리셋이 안 잡혀서 부팅이 안 된다)

### A-1. 스크립트 실행

Vivado 실행 후 Tcl Console에서:

```tcl
cd C:/Users/ehdgn/SOTA/zcutonpu/zcu102-coralnpu/01_hw
source rebuild_project.tcl
```

또는 명령 프롬프트에서 배치 실행:

```
vivado -mode batch -source rebuild_project.tcl
```

**소요 시간 40분 ~ 1시간 30분.** 끝나면 다음이 생성된다.

- `coral_zcu102/coralnpu_wrapper.xsa` ← Vitis에 넘길 파일
- `coral_zcu102/coral_zcu102.runs/impl_1/coralnpu_wrapper.bit`

### A-2. 확인할 것

- 콘솔 마지막에 **타이밍 WNS가 양수**인지 (2026-07-23 기록: `+0.462`)
  음수면 `rebuild_project.tcl`의 `PL_FREQ`를 50 → 40으로 낮추고 재실행
- **Address Editor에서 coral s_axi = `0x5_0000_0000`** 인지
  이 값이 `03_sw/main_mnist.c`의 `#define CB 0x0000000500000000ULL`과
  다르면 소프트웨어가 전부 동작하지 않는다

---

## B. Vitis — coral_app.elf

`docs/LOG.md` 2026-07-23 / 07-27 기록 기준.

1. **Vitis Unified IDE 2026.1** 실행, 워크스페이스는 **한글·공백 없는 경로**로
   (예: `C:\Users\ehdgn\SOTA\zcutonpu\01_hw\vitis_ws`)

2. **Platform 생성** — 이름 `coral_platform`
   - XSA: A-1에서 만든 `coralnpu_wrapper.xsa`
   - OS: `standalone`
   - CPU: `psu_cortexa53_0`

3. **Application 생성** — 이름 `coral_app`
   - 템플릿: `Empty Application (C)`
   - `coral_app/src/` 에 소스 추가
     - MNIST 데모: `03_sw/main_mnist.c`
     - CNN 데모: `03_sw/main_cnn.c`
   - **둘 중 하나만** 넣을 것 (`main`이 둘이면 링크 에러)

4. **Build** → `coral_app.elf` 생성

### 함정 (전에 겪은 것)

- `main.c`를 platform 쪽에 만들면 → `ninja: no work to do`
  반드시 **application의 Sources** 아래에 둘 것
- 빈 `main.c`가 컴파일되면 → `undefined reference to 'main'`
  코드를 넣고 **저장**한 뒤 다시 빌드

---

## C. bootgen — BOOT.bin

Vitis → `Vitis` 메뉴 → **Create Boot Image** (Zynq UltraScale+)

구성 순서가 중요하다.

| 순서 | 파일 | 타입 |
|---|---|---|
| 1 | `fsbl.elf` (platform이 자동 생성) | **bootloader** |
| 2 | `coralnpu_wrapper.bit` | datafile |
| 3 | `coral_app.elf` | datafile |

- Output Image: `.../coral_app/BOOT.bin`
- 결과물 약 26MB
- **PMU 펌웨어는 없어도 된다.** 부팅 시 `PMU-FW is not running` 경고가
  뜨지만 앱은 정상 동작함이 확인됨 (2026-07-27)

> 참고: 이 프로젝트 초기에 겪은 부팅 실패는 ITRI의 **2018.3 FSBL**이
> 보드 Rev 1.1과 안 맞아서였다. 지금처럼 **2026.1로 직접 생성한 FSBL**을
> 쓰면 그 문제는 재발하지 않는다.

---

## D. SD 부팅 (라이선스 불필요)

1. SD카드를 **FAT32**로 포맷
2. 루트에 `BOOT.bin` 복사 (기존 `image.ub`가 있으면 삭제)
3. 보드 스위치 **SW6: 1-ON / 2·3·4-OFF** (SD 부팅 모드)
4. USB-UART 연결, **PuTTY 115200 8N1** (COM 포트는 장치 관리자에서 확인, Silicon Labs)
5. 전원 ON → FSBL → 비트스트림 로딩 → `coral_app` 자동 실행

정상이면 UART에 이렇게 나온다.

```
===== MNIST handwritten digit recognition on Coral NPU =====
model 64-32-10 int8  |  float32 acc 0.980 -> int8 acc 0.980
...
===== 10 / 10 correct =====
```

기대 출력 전문은 `05_results/uart_capture/mnist_uart.txt` 참고.

### 부팅이 안 될 때

- COM 포트가 안 보임 → USB 재연결 후 전원 ON (2026-07-28에 겪음)
- FSBL 배너만 뜨고 멈춤 → SW6 스위치 위치, SD 포맷(FAT32) 확인
- 아무것도 안 뜸 → PuTTY 보드레이트 115200, 포트 번호 확인

---

## 전체 함정 요약

작업 순서대로 겪었던 것들. 스크립트에는 이미 반영돼 있다.

| # | 함정 | 대응 |
|---|---|---|
| 1 | `SYNTHESIS` define 없으면 DPI-C 시뮬 전용 경로 선택 → 합성 실패 | Tcl에서 자동 설정 |
| 2 | `ClockGate.sv`/`RstSync.sv`를 따로 추가 → 모듈 중복 정의 | `CoreMiniAxi.sv`에 내장, 추가 금지 |
| 3 | Top을 `coral_axi_wrapper`로 두면 **870 I/O > 707** 에러 | Top = `coralnpu_wrapper` |
| 4 | GUI Add Module이 "Module references are still updating"에서 멈춤 | Tcl `create_bd_cell -type module` |
| 5 | 100MHz는 타이밍 위반 (WNS -2.385) | PL_CLK0 = 50MHz |
| 6 | irq 자동 인터럽트 연결 실패 | Constant(0)로 고정 |
| 7 | Vitis 내장 터미널 "Access denied" | PuTTY 사용 |
| 8 | 한글·공백 경로에서 Vitis가 파일을 못 읽음 | 영문 경로 사용 |

---

## 완료 후 반드시 할 것

같은 사고를 반복하지 않기 위해, **산출물을 GitHub 밖에 백업**한다.
(`.gitignore`가 `.bit`/`.bin`을 제외하므로 커밋으로는 안 남는다)

- `BOOT.bin`, `coralnpu_wrapper.xsa`, `coralnpu_wrapper.bit`
- → OneDrive 또는 USB에 복사
- SD카드 자체도 하나 더 만들어 예비로 보관

BOOT.bin만 살아있으면 PC가 또 고장나도 데모는 돌아간다.
