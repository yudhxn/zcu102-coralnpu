# 내일 아침 — 5분 체크리스트

## 0. (선택·권장) 폴더 이름 변경 — 프로젝트명 통일
문서상 경로는 `zcu102-coralnpu` 기준으로 통일했다. 실제 폴더도 맞추려면:
```
Rename-Item C:\Users\ehdgn\SOTA\zcutonpu\zcu102-nvdla zcu102-coralnpu
```
(Cowork 세션이 폴더를 잡고 있으면 실패할 수 있음 — 그 경우 Claude 앱 종료 후 실행.
스크립트들은 자기 위치 기준으로 동작하므로 이름을 안 바꿔도 전부 정상 동작한다.)

물리 작업은 SD 교체뿐. 나머지는 전부 준비돼 있음.

## 출근해서 (5분)

1. **SD를 PC에 꽂기** → 탐색기에서 드라이브 문자 확인 (예: E:)
2. **한 줄 실행**:
   ```
   powershell -ep bypass -File C:\Users\ehdgn\SOTA\zcutonpu\zcu102-coralnpu\09_remote\make_sd.ps1 E:
   ```
   (리눅스 부팅파일 + NPU 추론 일체를 SD에 복사. 기존 베어메탈 BOOT.bin은
   자동으로 `BOOT_baremetal_mnist.bak`로 보존됨)
3. **SD를 보드에 꽂고 전원 ON** (SW6 그대로: 1-ON / 2,3,4-OFF)
4. (되면) 보드에 **랜선** 연결 — SSH/CI용. 안 꽂아도 시리얼로는 다 됨
5. 퇴근 전 PC 켜두기 (원격 데스크톱용)

이게 전부. 아래는 사무실에서 하든, 집에서 원격으로 하든 무관.

## 실행 (사무실 or 집에서 원격 데스크톱 → PuTTY COM3)

리눅스 부팅 후 (약 30초, `zynqmp login:` 프롬프트):

```
root          ← 아이디
root          ← 비번
sh /run/media/mmcblk0p1/board/run_demo.sh 8
```

스크립트가 자동으로: **보드 IP 표시** → Coral 비트스트림 로드 → PL클럭 50MHz
→ AXI 탐침 → 8×8 MNIST 추론.

- `10 / 10 correct` → 전체 경로 검증 완료. 이어서:
  ```
  sh /run/media/mmcblk0p1/board/run_demo.sh 28      # 28x28 데모
  sh /run/media/mmcblk0p1/board/run_demo.sh full    # 10,000장 전수 → 9641/10000이면 PASS
  ```
- 에러/이상한 출력 → **출력 전체를 복사해서 Claude에게** 붙여넣기.
  (probe 결과만 있으면 원인 진단 가능. 예상 시나리오별 대응은 README_OVERNIGHT.md)

## 그 다음 (선택)

- 시리얼에 표시된 **보드 IP 메모** → `ssh root@IP` 확인
- GitHub CI 연결: `ci/RUNNER_SETUP.md` (PC에 러너 설치 + Secrets 2개)
  → 이후 `git push`마다 실제 보드에서 추론 자동 실행
- GitHub에 `09_remote/` 커밋·푸시 (아직 안 올라가 있음)

## 되돌리기 (만약을 위해)

리눅스 경로가 어떤 이유로든 막히면:
SD의 `BOOT_baremetal_mnist.bak` → `BOOT.bin`으로 이름 바꾸고 `image.ub` 삭제
→ 기존 베어메탈 8×8 데모로 즉시 복귀. **8/31 발표는 어떤 경우에도 안전.**
