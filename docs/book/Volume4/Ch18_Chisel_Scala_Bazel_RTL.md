# Chapter 18. Chisel · Scala · Bazel · RTL — 하드웨어를 프로그래밍하다

---

## 18.1 RTL — 하드웨어 기술의 표준 높이

**RTL**
/ˌɑːrtiːˈɛl/
알티엘 (Register Transfer Level)
**"클럭마다 레지스터 사이를 데이터가 어떻게 이동·변환되는가" 수준의 하드웨어 기술.**

12장의 그림(FF → 조합논리 → FF)이 정확히 RTL의 세계관입니다. 게이트 하나하나(너무 낮음)도, "행렬을 곱해라"(너무 높음)도 아닌, **합성기가 자동 처리할 수 있는 딱 좋은 높이.**

```verilog
// Verilog RTL 예: 1클럭 MAC
always @(posedge clk)        // 클럭 상승 에지마다
  acc <= acc + (a * b);      // acc 레지스터 ← 기존값 + 곱
```

- `always @(posedge clk)` — "다음은 FF에 저장되는 동작"을 선언.
- `acc <= ...` — 비차단 대입. 이번 에지에 계산된 값이 **다음 상태**가 됨. `acc`는 FF로, `a*b`는 곱셈기(DSP 추론 후보!)로, `+`는 가산기로 합성됩니다.

## 18.2 Verilog의 한계와 Chisel의 등장

**Verilog** /ˈvɛrɪlɒɡ/ 베릴로그 — 1984년생 하드웨어 기술 언어. 표준이지만 설계 규모가 커지면 괴로워집니다: 파라미터화가 약하고, 반복 구조를 추상화하기 어렵고, 타입 검사도 느슨합니다.

**Chisel**
/ˈtʃɪzəl/
치즐 ("끌"이라는 뜻)
**Scala 언어 위에서 하드웨어를 기술하는 라이브러리.** 2012년 UC 버클리(RISC-V와 같은 뿌리!)에서 개발.

**Scala** /ˈskɑːlə/ 스칼라 — JVM 위의 함수형+객체지향 언어. Chisel의 숙주.

### 핵심 개념: 하드웨어 생성기(generator)

```scala
// Chisel: 파라미터화된 MAC 배열
class MacArray(n: Int, width: Int) extends Module {
  val io = IO(new Bundle {
    val a = Input(Vec(n, SInt(width.W)))     // 입력 n개 묶음
    val b = Input(Vec(n, SInt(width.W)))
    val acc = Output(SInt((2*width+8).W))
  })
  val products = io.a.zip(io.b).map { case (x, y) => x * y }  // n개 곱셈기 생성
  io.acc := RegNext(io.acc + products.reduce(_ + _))          // 가산 트리 + FF
}
```

한 줄씩:
- `class MacArray(n: Int, ...)` — **n이 코드 변수**. n=16이면 곱셈기 16개, n=64면 64개가 **생성**됩니다. Verilog라면 복붙 지옥.
- `Vec(n, SInt(width.W))` — 부호 있는 정수 n개 묶음 포트.
- `.map { x * y }` — Scala의 함수형 반복이 **회로 n개 인스턴스화**로 번역됨.
- `.reduce(_ + _)` — 곱들을 더하는 **가산 트리**가 자동 생성.
- `RegNext(...)` — FF 한 단. Verilog의 `always @(posedge clk)`에 해당.

Chisel 코드를 실행하면(Scala 프로그램이므로 "실행"됩니다) **Verilog가 출력**됩니다. 그래서 CoreMiniAxi.sv가 36,725줄짜리 기계 생성물인 것 — 사람이 읽을 물건이 아니고, 읽을 필요도 없으며, **디버깅은 Chisel 원본에서** 합니다.

### 왜 구글이 Chisel을 썼나

NPU는 "MAC 몇 개, 벡터 폭 몇 비트, TCM 몇 KB"라는 **규모 파라미터의 제품군**입니다. 생성기 접근이면 파라미터만 바꿔 파생 설계를 찍어낼 수 있습니다. RISC-V 진영(버클리 계보)의 표준 도구라는 궁합도 있고요.

### 부작용 — 우리가 겪는 것

생성된 Verilog는 **합성기 친화적이지 않을 수 있습니다.** 곱셈이 `*` 연산자가 아니라 명시적 논리로 전개되어 나오면 Vivado의 DSP 추론(12장)이 실패합니다. **"DSP 6개" 미스터리의 도구 수준 원인 후보**가 바로 이것입니다.

## 18.3 Bazel — 생성 파이프라인의 관리자

**Bazel** /ˈbeɪzəl/ 베이젤 — 구글 내부 도구 Blaze의 공개판(2015). 핵심 개념:

- **재현성**: 같은 입력이면 언제나 같은 출력 (샌드박스 빌드).
- **의존 그래프**: "Chisel 컴파일 → Verilog 생성 → Verilator 컴파일"의 순서를 자동 관리.
- **타깃 분리**: RISC-V용(examples)과 x86용(verilator_sim)은 다른 설정 → **다른 출력 폴더** (`bazel-out/k8-fastbuild-ST-.../` vs `k8-fastbuild/`). `bazel-bin` 심볼릭 링크는 최근 빌드를 가리켜 **이동**하므로 절대 경로를 잡아야 함 — 우리가 겪은 삽질의 원리.

```
bazel build //examples:...hello_world...     # RISC-V .elf/.bin/.vmem (222s)
bazel build //tests/verilator_sim:core_mini_axi_sim   # Chisel→Verilog→시뮬레이터 (368s)
```

두 번째 명령의 로그 `core_mini_axi_cc_library_emit_verilog`가 **Chisel→Verilog 변환의 실행 증거**였고, 그 산출물이 M3 합성의 입력이 됐습니다.

## 18.4 요약·퀴즈

- RTL = FF 사이 데이터 이동 수준의 기술. 합성 자동화의 표준 높이.
- Chisel = Scala 위의 하드웨어 **생성기**. 파라미터로 회로 규모를 찍어냄. 실행하면 Verilog가 나옴.
- 생성된 Verilog는 DSP 추론에 불리할 수 있음 → 우리 프로젝트 미스터리의 후보 원인.
- Bazel = 재현성 있는 빌드 그래프 관리자. 타깃별 출력 분리와 symlink 이동이 삽질 포인트.

**Q (교수님). "Chisel을 쓰면 뭐가 좋고 뭐가 나빠지나?"**
A. 파라미터화·재사용·타입 안전성으로 설계 생산성이 크게 오릅니다. 반면 생성된 Verilog가 사람이 읽기 어렵고, 벤더 합성기의 추론 패턴(예: DSP 매핑)과 어긋날 수 있어 FPGA 백엔드 품질은 별도 검증이 필요합니다. 본 프로젝트의 DSP 미사용 관측이 그 실례 후보입니다.

**퀴즈**: ① Chisel 코드를 "실행"하면 나오는 것은? ② bazel-bin이 갑자기 다른 곳을 가리키는 이유는?
<details><summary>정답</summary>① Verilog(SystemVerilog) 코드 ② 심볼릭 링크가 가장 최근 빌드의 출력 폴더로 이동하므로</details>

---
**Volume 4 완결. 다음**: Volume 5 — 프로젝트 분석. 모든 장이 여기로 수렴한다.
