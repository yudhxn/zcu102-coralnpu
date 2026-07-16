
---

### YYYY-MM-DD (요일)
- **목표:**
- **한 일:**
- **막힌 것:**
- **해결/원인:**
- **다음:**

---

### 2026-07-14 (화)
- 한 일: GitHub 레포 세팅 / ITRI-OpenDLA 확보 / BOOT.bin 발견
        / CP210x 드라이버 설치 / 보드 첫 부팅 시도
- 막힌 것: FSBL 배너 3줄 후 정지. "PMU-FW is not running"
- 원인 추정: ITRI BOOT.bin = 2018.3 FSBL, 보드 = Rev 1.1 → 궁합 문제
- 다음: Xilinx 공식 프리빌트 이미지로 진단 확정


### 2026-07-15 (수)
[오전~오후: 보드 진단]
- 결과: AMD 2019.1 프리빌트 이미지로 부팅 → 로그인까지 성공 ✅
- 하드웨어 확인: 커널 4.19 aarch64 / CPU 4 / FPGA operating (전부 정상)
- 원인 확정: 멈춤 원인 = ITRI 2018.3 FSBL ↔ 보드 Rev 1.1 불일치
- 네트워크: IP 수동 할당(10.126.37.57) + SSH 접속 성공
  (ssh -o HostKeyAlgorithms=+ssh-rsa 옵션 필요, VS Code는 BusyBox라 불가)

[오후~저녁: 이론 + 파일 분석]
- 이론: 부팅 5단계 릴레이 이해 (BootROM→FSBL→PMU/ATF→U-Boot→Linux)
- .bif 분석: ITRI의 SD_BOOT.bif에 [pmufw_image] 줄이 없음
  → PMU 누락 확인, "PMU-FW is not running" + Rev1.1 실패의 직접 원인
- .hdf 확인: zcu102_base_trd_wrapper.hdf가 산2의 핵심 재료 (비트스트림 포함)
- Vitis 버전: PC는 2026.1이라 .hdf 호환 안 됨 → 서버에 2019.1 별도 설치 필요

- 다음: 서버 허가(VPN/계정/Vitis 2019.1 설치) 받기 → FSBL/PMU 생성 → BOOT.bin 재조립

### 2026-07-16일 — Coral NPU로 방향 전환
- 교수님 지시: NVDLA → Google Coral NPU (ZCU102에 Coral 올리기)
- 목표: 합성(FPGA) → 실제 추론까지
- 팩트체크: 레포 활발(별2.4k), fpga/platforms 폴더 존재, 매트릭스코어 M3 릴리스됨(2026-04-27)
  빌드=Bazel, 언어=SystemVerilog/Scala, Verilator 시뮬 가능
- 미확인: platforms에 ZCU102 지원 있는지 (난이도 좌우) → 서버에서 확인
- 재활용: 보드부팅/FSBL/PMU/bootgen/SSH/서버 다 그대로 쓰임 (특히 BOOT.bin 만들기)
- 다음: 교수님께 범위/마감 확인 + 서버 접속(VPN/계정) 받기 → platforms 확인 → 시뮬 M1
- 새 계획: 05_notes/PLAN_coral_7weeks.md