# Chapter 15. AXI4와 SmartConnect — 두 세계를 잇는 고속도로

---

## 15.1 AXI의 역사

**AXI**
/ˌeɪɛksˈaɪ/
에이엑스아이 (Advanced eXtensible Interface)
**ARM이 만든 칩 내부 버스 표준.** AMBA(ARM의 버스 규격 패밀리, 1996~) 3세대에서 등장, 현행 주력이 **AXI4** (2010).

### 왜 만들어졌는가

SoC 시대가 되며 한 칩에 수십 개 블록(CPU, 메모리, 가속기, 주변장치)이 동거하게 됐습니다. 블록마다 연결 방식이 다르면 조합이 불가능하죠. ARM이 표준 커넥터를 정의해 공개했고, 사실상 업계 표준이 됐습니다. **구글의 Coral과 Xilinx의 PS가 바로 연결되는 이유 = 둘 다 AXI라는 같은 커넥터를 채택**했기 때문.

## 15.2 manager와 subordinate

**manager** (구 master) — 요청하는 쪽. "이 주소 읽어줘 / 여기 써줘"
**subordinate** (구 slave) — 응답하는 쪽.

Coral은 **양쪽 다** 가집니다 (Vol.4에서 재론):

```
ARM(PS) ──요청──► io_axi_slave  ┐
                                 │ CoreMiniAxi
DDR ◄──요청── io_axi_master ────┘
```

## 15.3 5채널 구조

읽기와 쓰기가 **독립 채널**로 분리되어 있습니다.

| 채널 | 방향 | 내용 |
|---|---|---|
| **AW** (Write Address) | M→S | "어디에 쓸 것" |
| **W** (Write Data) | M→S | 쓸 데이터 |
| **B** (Write Response) | S→M | "다 썼음" |
| **AR** (Read Address) | M→S | "어디를 읽을 것" |
| **R** (Read Data) | S→M | 읽은 데이터 |

주소를 먼저 여러 개 보내놓고 데이터를 나중에 받는 **파이프라이닝**(9장의 개념이 버스에도!)이 가능하고, 읽기·쓰기가 동시 진행됩니다. 신호 2,866개의 출처 = 5채널 × (주소 32b + 데이터 128b + 제어) × 양방향.

### 핸드셰이크 — 모든 채널의 공통 규칙

```
VALID (보내는 쪽): "줄 것 준비됨"
READY (받는 쪽):   "받을 준비됨"
     둘 다 1인 클럭 에지에 전송 1건 성립
```

어느 쪽이 느려도 안전하게 기다립니다. 속도가 다른 블록(300MHz PS ↔ 100MHz PL)이 이어질 수 있는 이유.

## 15.4 Zynq의 AXI 포트들

| 포트 | 방향 | 용도 |
|---|---|---|
| **M_AXI_HPM0/1** | PS→PL | PS가 PL 레지스터 읽고 씀 (제어) |
| **S_AXI_HP0~3** | PL→PS | PL이 DDR 접근 (데이터, 128b) |
| S_AXI_HPC | PL→PS | 캐시 일관성 버전 (이번엔 불필요) |

우리 배선:

```
PS M_AXI_HPM0 (제어)  ──────► Coral io_axi_slave  (addr 32b / data 128b)
PS S_AXI_HP0  (데이터) ◄────── Coral io_axi_master (addr 32b / data 128b)
```

⭐ **HP 포트 폭 = 128b = Coral 폭.** 폭이 다르면 변환 IP(자원+지연+복잡도)가 필요한데, 일치해서 직결 가능. M3에서 확인한 유리한 조건입니다.

## 15.5 SmartConnect — 자동 교차로

**SmartConnect**
/smɑːrt kəˈnɛkt/
스마트커넥트
**Vivado가 제공하는 AXI 인터커넥트 IP.** 여러 manager/subordinate를 잇는 교차로를 자동 생성하고, 프로토콜 버전·폭·클럭 차이도 자동 변환.

Block Design에서 "Run Connection Automation"을 누르면 이 블록이 끼워지며 배선이 완성됩니다. 주소 라우팅("0xA0000000번대 요청은 Coral로")도 여기서 처리 — 그 주소표가 **Address Editor**입니다.

## 15.6 현재 프로젝트 연결

- M4 Block Design의 배선 그 자체. 자동화 도구가 SmartConnect를 삽입해 PS↔Coral을 잇는다.
- Address Editor에서 Coral에 준 주소가 `xparameters.h` → 베어메탈 코드의 `CORAL_BASE`.
- M4 완료 판정(레지스터 write→read 왕복)은 **AW·W·B 채널과 AR·R 채널이 모두 살아 있다**는 검증.

## 요약·퀴즈

- AXI4 = ARM의 표준 커넥터. 5채널 분리 + VALID/READY 핸드셰이크로 고성능·안전.
- Zynq: HPM(PS가 제어) / HP(PL이 DDR 접근). 우리는 HPM0+HP0, 폭 128b 직결.
- SmartConnect가 교차로·프로토콜 변환·주소 라우팅을 자동화.

**Q (교수님). "AXI에서 읽기 주소와 쓰기 주소 채널이 분리된 이유는?"**
A. 읽기와 쓰기를 독립적으로 파이프라이닝해 동시 진행할 수 있게 하기 위함입니다. 채널이 통합돼 있으면 한쪽 트랜잭션이 다른 쪽을 막아 대역폭이 떨어집니다.

**퀴즈**: ① 전송이 성립하는 조건은? ② 폭이 128b로 일치해서 생략된 IP는?
<details><summary>정답</summary>① VALID와 READY가 같은 클럭에 모두 1 ② AXI Data Width Converter</details>

---
**다음 장**: 부팅과 베어메탈 — BootROM부터 내 코드까지.
