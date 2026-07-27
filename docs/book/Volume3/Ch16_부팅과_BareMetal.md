# Chapter 16. 부팅과 Bare Metal — 전원 버튼부터 내 코드까지

---

## 16.1 부팅이 어려운 이유

전원이 켜진 직후의 칩은 **아무것도 모릅니다.** DDR도 초기화 전이라 못 쓰고, PL은 백지입니다. 이 상태에서 "누가 첫 코드를 실행하는가?" — 닭이 먼저냐 달걀이 먼저냐 문제를, **단계적 릴레이**로 풉니다. 각 주자는 다음 주자가 뛸 환경을 만들어주고 바통을 넘깁니다.

## 16.2 릴레이 전 구간

```
① 전원 ON
     ▼
② BootROM  (CSU에서 실행. 제조 시 각인, 수정 불가)
     · 부팅 모드 핀(SW6) 읽기 → "SD구나"
     · SD 첫 파티션(FAT32)에서 BOOT.bin 탐색
     · 헤더 검증 후 FSBL을 내부 RAM(OCM)에 적재
     ▼
③ PMU Firmware (PMUFW)
     · 전원 도메인 관리 시작. 이후 상주하며 전원·에러 감시
     ▼
④ FSBL (First Stage Boot Loader)
     · PS 초기화: 클럭, DDR 컨트롤러 (이제야 DDR 사용 가능!)
     · PL 설정: 비트스트림 로드 → 이 순간 Coral 회로 탄생
     · 다음 이미지(우리는 베어메탈 앱)를 메모리에 적재
     ▼
⑤ 베어메탈 앱 (a53-0에서 실행)
     · Coral 제어 → UART 출력
     
※ 리눅스 부팅이라면 ⑤ 자리에 ATF → U-Boot → Linux 커널이 들어감
```

**OCM** (On-Chip Memory) — PS 안의 작은 SRAM. DDR 초기화 전에도 쓸 수 있어 FSBL의 첫 무대가 됩니다.

## 16.3 BOOT.bin과 BIF

**BOOT.bin** — 릴레이 주자들을 하나로 묶은 컨테이너. BootROM이 아는 유일한 파일 형식.
**bootgen** — 조각들을 BOOT.bin으로 묶는 Xilinx 도구.
**BIF** (Boot Image Format) — bootgen에게 주는 **조립 설명서.**

우리 프로젝트용 최소 BIF:

```
the_ROM_image:
{
  [fsbl_config] a53_x64          // FSBL을 A53 64비트 모드로
  [bootloader]  fsbl.elf          // ④ 주자
  [pmufw_image] pmufw.elf         // ③ 주자  ⭐ 절대 누락 금지
  [destination_device = pl] system.bit   // 비트스트림 → PL로
  [destination_cpu = a53-0] app.elf      // ⑤ 주자 → a53-0으로
}
```

한 줄씩:
- `[fsbl_config] a53_x64` — FSBL 실행 모드 지정.
- `[bootloader]` — 이 파일이 FSBL임을 표시. BootROM이 제일 먼저 올릴 대상.
- `[pmufw_image]` — PMU 펌웨어 포함. **이 줄이 없으면 PMU가 안 떠서 이후 단계가 좌초** (아래 사건).
- `[destination_device = pl]` — 이 파일은 PL로 보내라 = 비트스트림.
- `[destination_cpu = a53-0]` — 이 ELF는 A53 0번 코어에서 실행하라 = 우리 앱.

## 16.4 사례 연구 — 7/14 부팅 실패 사건 ⭐

**증상** (UART 출력):

```
Xilinx Zynq MP First Stage Boot Loader
Release 2018.3
PMU-FW is not running, certain applications may not be supported.
(이후 정지)
```

**진단 과정** — 교과서적 순서였습니다:

```
1. 대조 실험: AMD 공식 2019.1 프리빌트 → 정상 부팅
   → 보드·SD·UART·스위치 무죄. 원인은 소프트웨어로 좁혀짐

2. 증거 수집: ITRI의 SD_BOOT.bif를 직접 열람
   → [pmufw_image] 항목 자체가 없음

3. 증상-원인 대응: 로그 "PMU-FW is not running" ↔ BIF의 PMU 누락
   → 인과 확정
```

**해법**: 비트스트림은 유지하고, 새 FSBL + PMUFW를 포함해 bootgen 재조립.

> **교훈 두 개.**
> ① 부팅 실패는 "죽은 지점의 직전 주자"를 의심하라 — FSBL 배너까지 나왔으니 FSBL은 떴고, 그다음 의존물(PMU)이 문제였다.
> ② 로그의 경고 문구는 그냥 나오는 게 아니다 — 설정 파일과 대조하면 원인을 특정할 수 있다.

## 16.5 Bare Metal — OS 없이 산다는 것

**Bare Metal** /bɛər ˈmɛtəl/ 베어 메탈 — **OS 없이 하드웨어 위에서 직접 도는 프로그램.**

| | 리눅스 경로 | 베어메탈 경로 (우리) |
|---|---|---|
| 추가 주자 | ATF, U-Boot, 커널, rootfs | 없음 |
| 하드웨어 접근 | 드라이버 + 디바이스 트리 | **주소 직접 읽기/쓰기 (MMIO)** |
| 개발 도구 | PetaLinux (리눅스 서버) | **Vitis (윈도우 OK)** |
| 장점 | 풍부한 기능 | **단순, 빠른 개발, 완전한 제어** |

선택 근거: Coral은 프로세서라 드라이버가 불필요(Vol.1)하고, 목표가 "input/output 동작 확인"이므로 OS의 기능이 필요 없습니다. **부수 효과로 리눅스 서버·VPN 의존이 통째로 사라졌습니다.**

### 베어메탈 앱의 뼈대

```c
#include "xparameters.h"          // Vitis가 .xsa에서 자동 생성한 주소들
#include "xil_io.h"               // Xil_In32/Out32
#include "xil_printf.h"           // 경량 printf (UART로 출력)

#define CORAL_BASE  XPAR_CORALNPU_0_BASEADDR   // Address Editor에서 정한 주소

int main(void) {
    xil_printf("Coral bring-up\r\n");          // ① 살아있음을 UART로 알림
    Xil_Out32(CORAL_BASE + 0x0, 0xDEADBEEF);   // ② 아무 레지스터에 값 쓰기
    u32 v = Xil_In32(CORAL_BASE + 0x0);        // ③ 되읽기
    xil_printf("readback = %08x\r\n", v);      // ④ 같으면 AXI 왕복 성공 = M4 달성
    while (1);                                  // ⑤ 베어메탈은 return할 곳이 없다
}
```

한 줄씩:
- ①: UART 115200으로 문자가 나오면 "⑤ 주자까지 릴레이 성공".
- ②③: AW·W·B 채널(쓰기)과 AR·R 채널(읽기)을 모두 검증하는 최소 실험.
- ④: `0xDEADBEEF`가 그대로 돌아오면 **PS↔PL AXI 경로 완전 개통.**
- ⑤: OS가 없으므로 main이 끝나면 갈 곳이 없다 → 무한 루프가 관례.

## 요약·퀴즈

- 부팅 = 단계적 릴레이: BootROM → PMUFW → FSBL(DDR·비트스트림) → 앱. BOOT.bin이 주자 명단, BIF가 그 명세.
- 7/14 사건: BIF에 `[pmufw_image]` 누락 → 로그 경고와 대조해 확정. 대조 실험 → 증거 열람 → 인과 대응의 3단 진단.
- 베어메탈 = MMIO 직접 제어. 레지스터 write-readback이 M4의 합격 판정.

**Q (교수님). "FSBL 배너는 나왔는데 멈췄다. 어디를 의심하나?"**
A. 배너가 나왔으므로 BootROM→FSBL 적재까지는 성공입니다. 직후 의존 요소 — PMU 펌웨어 유무, DDR 초기화, 다음 이미지 적재 — 를 순서대로 의심합니다. 실제 사례에서는 로그의 "PMU-FW is not running" 경고와 BIF의 [pmufw_image] 누락이 대응되어 원인이 확정됐습니다.

**퀴즈**: ① BootROM이 수정 불가인 이유는? ② FSBL의 3대 임무는? ③ 베어메탈 main 끝의 무한 루프 이유는?
<details><summary>정답</summary>① 제조 시 실리콘에 각인되므로 ② PS 초기화(클럭·DDR), 비트스트림 로드, 다음 이미지 적재 ③ 돌아갈 OS가 없어서</details>

---
**Volume 3 완결. 다음**: Volume 4 — Google Coral NPU 해부.
