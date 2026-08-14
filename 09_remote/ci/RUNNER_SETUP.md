# GitHub CI → 실제 보드 추론 연결 가이드 (2026-08-13 갱신)

```
git push → GitHub Actions → 연구실 PC(러너) → plink(SSH) → ZCU102 → Coral NPU
```

러너를 보드가 아니라 **PC에 두는 이유**: 보드 rootfs 가 initramfs(램)라
재부팅하면 설치물이 사라지고, GitHub 러너가 요구하는 glibc/ICU 도 없다.

## 0. 전제 조건 확인

CI 를 붙이기 전에 아래가 되어 있어야 한다.

| 항목 | 확인 방법 |
|---|---|
| 보드가 고정 IP 로 뜬다 | PC 에서 `ping 10.126.37.200` |
| SSH 로 들어가진다 | `ssh root@10.126.37.200` (비번 `root`) |
| 부팅 옵션이 이미지에 박혔다 | 보드에서 `cat /proc/cmdline` 에 `clk_ignore_unused` 와 `pd_ignore_unused` |
| SD 에 board 폴더가 있다 | 보드에서 `ls /run/media/mmcblk0p1/board` |

**부팅 옵션 두 개가 핵심이다.** 없으면 NPU 접근 순간 보드가 멈추거나
SError 로 커널 패닉이 난다. (커널이 미사용 클럭·전원 도메인을 자동으로 끄기 때문)

## 1. PC 에 self-hosted 러너 설치 (10분, 1회)

### ★ 설치 위치 — 사용자 폴더 안에 두지 말 것

러너를 서비스로 돌리면 `NT AUTHORITY\NETWORK SERVICE` 계정으로 실행된다.
이 계정은 `C:\Users\<이름>\...` 아래를 읽을 권한이 없어서 서비스가 즉시 죽는다.

```
System.UnauthorizedAccessException: Access to the path
'C:\Users\ehdgn\SOTA\zcutonpu\zcu102-nvdla' is denied.
```

**`C:\actions-runner` 처럼 사용자 폴더 밖에 설치할 것.**
(git 저장소 안에 두는 것도 피할 것 — 러너 파일이 350MB 넘는다)

임시로 검증만 할 거라면 서비스 대신 로그인 계정으로 직접 띄우면 된다.
```
.\run.cmd
```


1. 리포 → **Settings → Actions → Runners → New self-hosted runner**
2. **Windows / x64** 선택 → 나오는 명령을 PowerShell 에 그대로 붙여넣기
3. `config.cmd` 가 **labels** 를 물으면 → `zcu102-host` 입력
   (워크플로가 이 라벨로 러너를 찾는다)
4. 서비스로 등록해야 로그아웃·재부팅에도 살아있다:
   ```
   .\svc.cmd install
   .\svc.cmd start
   ```

## 2. 리포 Secrets 등록 (1회)

Settings → Secrets and variables → Actions → New repository secret

| 이름 | 값 |
|---|---|
| `BOARD_IP` | `10.126.37.200` |
| `BOARD_PW` | `root` |

## 3. plink 호스트키 캐시

PC PowerShell 에서 한 번 수동 접속해 `y` 로 키를 저장한다.

```
plink root@10.126.37.200
```

### ★ 보드를 재부팅하면 호스트키가 바뀐다

rootfs 가 initramfs(램)라 **부팅할 때마다 SSH 호스트키가 새로 생성**된다.
그래서 재부팅 후에는 PC 의 캐시와 달라져 `-batch` 모드가 멈춘다.

워크플로 첫 단계에서 `echo y | plink ...` 로 새 키를 자동 수락하게 해뒀으므로
CI 는 재부팅해도 그대로 동작한다. 다만 **수동으로 ssh 할 때는** 아래로 정리해야 한다.

```
ssh-keygen -R 10.126.37.200
```

호스트키를 고정하려면 SD 에 키를 보관했다가 부팅 시 복원하는 절차가 필요하다.
연구/데모 환경에서는 위 자동 수락으로 충분하다.

plink 는 PuTTY 설치 시 함께 깔린다. 없으면 putty.org 에서 `plink.exe` 만 받아
PATH 에 넣어도 된다.

## 4. 동작 확인

리포 → **Actions → NPU inference on real ZCU102 → Run workflow**
- mode `28` 로 먼저 (데모 10장, 몇 초)
- 되면 mode `full` (10,000장 전수, 1분 이내)

성공하면 이후로는 `09_remote/**` 를 건드려 push 할 때마다 자동 실행되고,
UART 로그가 artifact 로 남는다.

## 워크플로가 검증하는 것

| 단계 | 실패 조건 |
|---|---|
| 보드 연결 | SSH 접속 불가 |
| 부팅 옵션 | `/proc/cmdline` 에 `clk_ignore_unused` 또는 `pd_ignore_unused` 없음 |
| 추론 | mode full 에서 `9641 / 10000` 이 아니거나 `완전 일치` 문구 없음 |

즉 **하드웨어가 에뮬레이터와 한 장도 어긋나지 않아야 통과**한다.

## 문제 발생 시

| 증상 | 원인 / 대응 |
|---|---|
| 러너가 offline | PC 재부팅 후 `svc.cmd start` 확인 |
| `Host key verification` 에서 멈춤 | 3번(호스트키 캐시)을 안 한 것 |
| `clk_ignore_unused 없음` 으로 실패 | `fix_linux2.sh` 재실행 후 SD 재복사 |
| SSH 접속 불가 | 보드 전원/랜선 확인, `ping 10.126.37.200` |
| 보드가 멈춤 | `coral_probe` 를 인자 없이 실행한 경우. `rdprobe` 를 쓸 것 |
