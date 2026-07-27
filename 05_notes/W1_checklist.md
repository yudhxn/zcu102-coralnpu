# W1 준비물 체크리스트 (서버 붙으면 바로)

> 서버 접속되는 순간부터 순서대로. 위에서 막히면 아래로 못 감.

## 0. 서버 접속 (관리자한테 받아야)
- [ ] VPN 연결 방법 + 계정
- [ ] Compute 서버(10.125.19.139) 로그인 ID/PW
- [ ] Vitis/Vivado 사용 가능 여부 + 디스크 여유 확인 (100GB+)
- [ ] SSH 접속 성공: `ssh 계정@10.125.19.139` (VPN 연결 후)

## 1. 도구 설치 확인 (서버에 이미 있을 수도)
각각 버전 확인부터:
- [ ] `bazel --version` → 없으면 7.4.1 설치 필요
- [ ] `python3 --version` → 3.9~3.12 여야 함 (3.13 아직 미지원)
- [ ] `srec_cat --version` → SRecord 설치 확인
- [ ] `git --version`
- [ ] `vivado -version` → FPGA 합성용 (W3에서 씀)

> ⚠️ 공유 서버라 sudo 함부로 못 씀. 관리자한테 "이거 깔아주세요" 하거나
>    홈 디렉토리에 설치. 재부팅 절대 금지.

## 2. 레포 받기
- [ ] `cd ~` (홈으로)
- [ ] `git clone https://github.com/google-coral/coralnpu.git`
- [ ] `cd coralnpu`

## 3. 제일 먼저 확인할 것 (프로젝트 난이도 결정!)
- [ ] `ls platforms/` → **ZCU102 관련 폴더가 있는가?**
- [ ] `ls fpga/` → FPGA 빌드 스크립트 구조 파악
- [ ] `ls fpga/rtl/` → RTL 소스 확인
- [ ] `cat README.md` → System Requirements 재확인
    → 여기 결과를 캡처해서 Claude한테 보여줄 것. 계획이 여기서 갈림.

## 4. 시뮬 첫 실행 (M1 목표)
README Quick Start 그대로:
- [ ] `bazel run //tests/cocotb:core_mini_axi_sim_cocotb` (테스트 통과 확인)
- [ ] `bazel build //examples:coralnpu_v2_hello_world_add_floats`
- [ ] `bazel build //tests/verilator_sim:core_mini_axi_sim`
- [ ] 시뮬레이터로 바이너리 실행 (README 마지막 줄 명령)
- [ ] ✅ M1 달성: hello_world가 시뮬에서 돌면 성공

## 막히면 흔한 것
- Bazel 버전 안 맞음 → `.bazelversion` 파일 보고 정확히 그 버전
- Python 3.13이면 → 3.12로 낮추기 (venv)
- 첫 bazel build는 의존성 다운로드로 오래 걸림 (정상)
- 네트워크 막힘 → 서버 프록시/방화벽 확인 (관리자)

## LOG 남기기
- [ ] 각 단계 결과를 docs/LOG.md에 (특히 platforms 폴더 확인 결과)
