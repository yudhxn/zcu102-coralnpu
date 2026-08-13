# GitHub CI → 실제 보드 추론 연결 가이드

구조: **러너는 연구실 PC(Windows)에, 보드는 SSH로.**
보드(PetaLinux 2019.1)는 initramfs라 재부팅하면 설치물이 사라지고 러너
구동 요건(glibc/ICU)도 안 맞습니다. 항상 켜져 있는 PC에 러너를 두고
PC→보드는 PuTTY의 `plink`로 명령을 보내는 구조가 가장 견고합니다.

```
git push → GitHub Actions → 연구실 PC(러너) → plink(SSH) → ZCU102 보드
                                                             └ Coral NPU 추론
```

## 1. PC에 self-hosted 러너 설치 (약 10분, 1회)

1. GitHub 리포(`zcu102-coralnpu`) → **Settings → Actions → Runners → New self-hosted runner**
2. **Windows / x64** 선택 → 나오는 명령 3~4줄을 PowerShell에 그대로 붙여넣기
   (다운로드 → `config.cmd --url ... --token ...`)
3. `config.cmd` 질문 중 **labels** 물을 때: `zcu102-host` 입력 (워크플로가 이 라벨을 찾음)
4. 서비스로 설치해야 재부팅·로그아웃에도 살아있음:
   ```
   .\svc.cmd install
   .\svc.cmd start
   ```

## 2. 리포 Secrets 등록 (1회)

Settings → Secrets and variables → Actions → New repository secret

| 이름 | 값 |
|---|---|
| `BOARD_IP` | 보드 IP (예: 192.168.0.42) |
| `BOARD_PW` | `root` (기본값) |

## 3. plink 호스트키 캐시 (1회)

PC PowerShell에서 한 번 수동 접속해 `y` 눌러 키 저장:
```
plink root@<보드IP>
```
(plink는 PuTTY 설치 시 함께 설치됨. 없으면 putty.org에서 plink.exe 받기)

## 4. 보드 준비 (1회)

- SD FAT 파티션에 `09_remote/board/` 폴더 복사 (PC에서 pscp):
  ```
  pscp -pw root -r C:\Users\ehdgn\SOTA\zcutonpu\zcu102-coralnpu\09_remote\board root@<보드IP>:/run/media/mmcblk0p1/
  ```
  ※ SD에 직접 복사해도 됨 (FAT 파티션이 보드에서 /run/media/mmcblk0p1 로 마운트됨)

## 5. 동작 확인

- 리포 → Actions → **NPU inference on real ZCU102** → Run workflow (mode: `8`부터)
- 성공하면 `git push`만으로 실제 NPU 추론이 자동 실행되고,
  UART 로그가 artifact로 남습니다.

## 주의

- 보드 전원이 꺼져 있으면 워크플로는 "보드 연결 확인" 단계에서 실패합니다 (정상 동작).
- 보드 IP가 DHCP라 바뀌면 Secret만 갱신하면 됩니다.
- `mode: full`(10,000장)은 수 분 걸립니다.
