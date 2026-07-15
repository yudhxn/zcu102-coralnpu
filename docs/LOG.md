
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
- 결과: AMD 2019.1 프리빌트 이미지로 부팅 → 로그인까지 성공 ✅
- 하드웨어 확인: 커널 4.19 aarch64 / CPU 4 / FPGA operating (전부 정상)
- 원인 확정: 어제 멈춤 = ITRI 2018.3 FSBL ↔ 보드 Rev 1.1 불일치
- 네트워크: IP 수동 할당(10.126.37.57) + SSH 접속 성공
  (ssh -o HostKeyAlgorithms=+ssh-rsa 옵션 필요, VS Code는 BusyBox라 불가)
- 다음: 리눅스 서버 Vitis 2019.1 설치 →
