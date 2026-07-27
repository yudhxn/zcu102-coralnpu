# 02. Chisel → Verilog 툴체인

← [목차](00_INDEX.md) | 이전: [01. Coral NPU 구조](01_coral_npu_구조.md)

---

## 1. 문제 상황

Coral의 RTL은 **Verilog가 아니라 Chisel(Scala)로 작성**되어 있습니다.
**Vivado는 Chisel을 모릅니다.** 그래서 변환 단계가 필요해요.

**RTL** = Register Transfer Level. "레지스터 사이를 데이터가 어떻게 흐르는가" 수준으로 하드웨어를 기술하는 방식.

### 저장소 실사 결과 (07-20)

```
hdl/verilog/   → Sram.v, ClockGate.sv, RstSync.sv 등 부품 셀만 존재
hdl/chisel/    → NPU 본체 (Scala)
```

**본체가 Chisel에만 있다** → Bazel 빌드로 Verilog를 생성하지 않으면 합성 자체가 불가능.

---

## 2. Chisel이란

**Chisel** = Scala 언어로 하드웨어를 기술하는 방식.
정확히는 **"하드웨어를 만들어내는 프로그램"** 입니다.

### Verilog와의 차이

MAC 유닛 64개를 만든다고 하면:

```verilog
// Verilog — 복붙 64번
mac m0 (...);
mac m1 (...);
mac m2 (...);
// ... 61줄 더
```

```scala
// Chisel — 파라미터 하나
val macs = Seq.fill(numMacs)(Module(new MAC))
```

`numMacs`를 64로도 256으로도 바꿀 수 있습니다. **하드웨어의 크기를 코드로 조절**하는 것.

> **왜 Coral이 Chisel을 썼나**: NPU는 "MAC 몇 개, 벡터 폭 몇 비트" 같은 규모 파라미터가 핵심입니다. 용도마다 크기를 바꿔 찍어내야 하니 생성형 접근이 유리해요.

### 부작용 — 생성물이 사람이 읽을 물건이 아님

M3에서 합성한 `CoreMiniAxi.sv`는 **36,725줄 / 1.6MB** 짜리 단일 파일입니다.

> **기계가 생성한 코드**라서 읽기 어렵고, **읽을 필요도 없습니다.** 원본은 Chisel 쪽이에요. 디버깅이 필요하면 Chisel을 보는 게 맞습니다.

---

## 3. 전체 흐름

```
hdl/chisel (Scala 코드)
      │
      │  Bazel 빌드   ← WSL2 / Ubuntu 22.04 (리눅스)
      ▼
CoreMiniAxi.sv (SystemVerilog, 36,725줄 / 1.6MB)
      │
      │  Vivado 2026.1   ← Windows
      ▼
bitstream (.bit)
      │
      │  bootgen   ← NVDLA 단계에서 습득한 자산
      ▼
BOOT.bin → SD카드 → ZCU102
```

**리눅스와 윈도우가 역할을 나눠 가진다**는 게 이 프로젝트 환경의 특징입니다.

| 단계 | 실행 환경 | 이유 |
|---|---|---|
| Chisel → Verilog | **WSL2 (리눅스)** | Bazel이 리눅스 기준 |
| Verilog → bitstream | **Windows** | Vivado 2026.1 설치 위치 |

---

## 4. Bazel

**Bazel** = 구글이 만든 빌드 도구.

빌드 도구는 "무엇을 먼저 만들고 무엇을 나중에 만들지" 의존 관계를 자동 관리합니다. 파일이 수천 개인 프로젝트에서 사람이 순서를 짜는 건 불가능해요.

### bazelisk

**`bazelisk`** = Bazel 버전을 자동으로 맞춰주는 실행기(launcher).

```
README 안내:      Bazel 7.4.1
.bazelversion:    8.6.0        ← 실제 요구 버전
```

**불일치처럼 보이지만 문제없습니다.** `bazelisk`가 `.bazelversion`을 읽고 해당 버전을 알아서 내려받아 실행하기 때문. (README가 갱신이 안 된 것뿐)

> `/usr/local/bin/bazel`에 bazelisk를 두었으니, `bazel` 명령이 곧 bazelisk를 부릅니다.

---

## 5. WSL2

**WSL2** = Windows Subsystem for Linux 2. **윈도우 안에서 리눅스를 돌리는 기능.**

- 듀얼부팅이 **아님** — 윈도우를 켠 채로 리눅스가 같이 돎
- 재부팅 없이 두 환경을 오갈 수 있음

### ⚠️ 저장소 위치가 성능을 가른다

```
~/coralnpu          ← ✅ 리눅스 홈. 빠름
/mnt/c/Users/...    ← ❌ 윈도우 경로. 느림
```

**이유**: `/mnt/c`는 리눅스가 윈도우 파일시스템을 **번역해서** 접근하는 경로입니다. 파일 하나하나마다 변환 비용이 붙어요. 빌드는 파일을 수만 번 읽으므로 그 차이가 누적돼 몇 배로 벌어집니다.

> **일반 원칙**: WSL에서 빌드할 프로젝트는 **리눅스 홈에 둔다.**

### 설치 시 겪은 문제

```
초기 OOBE(초기 설정 화면) 멈춤
  → wsl --unregister 후 재설치로 해결
```

**OOBE** = Out-Of-Box Experience. 첫 실행 시 사용자 계정을 만드는 설정 화면. 여기서 멈추면 그 배포판을 등록 해제하고 다시 까는 게 가장 빠릅니다.

---

## 6. 설치한 것들

| 패키지 | 용도 |
|---|---|
| `build-essential` | gcc, make 등 컴파일 기본 도구 묶음 |
| `git` | 소스 관리 |
| `python3` (3.10) | Coral 요구: 3.9–3.12 |
| `srecord` | 바이너리 형식 변환 (.bin ↔ .vmem 등) |
| `curl`, `zip`, `unzip` | 다운로드·압축 |
| `bazelisk` → `/usr/local/bin/bazel` | 빌드 |

---

## 7. M1 결과

```
bazel build //examples:coralnpu_v2_hello_world_add_floats   (222초)
  → .elf / .bin / .vmem 생성
```

**의미**: RISC-V 크로스 컴파일 경로가 동작한다는 확인.

**크로스 컴파일** = 지금 쓰는 컴퓨터(x86)에서 **다른 종류의 CPU(RISC-V)** 용 프로그램을 만드는 것. Coral은 RISC-V라서 x86 PC에서 직접 실행되는 바이너리를 만들면 안 됩니다.

| 파일 | 용도 |
|---|---|
| `.elf` | 디버그 정보를 포함한 실행 파일 |
| `.bin` | 순수 기계어만 뽑은 것 |
| `.vmem` | 메모리 초기화용 텍스트 형식. **시뮬레이터·하드웨어 메모리에 부어넣을 때 사용** |

---

## 다음

→ [`03_verilator_시뮬레이션.md`](03_verilator_시뮬레이션.md) — 생성된 Verilog가 실제로 도는지 보드 없이 확인하기
