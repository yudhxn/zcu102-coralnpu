# SD카드 한 방 준비 스크립트 — 내일 아침 이것만 실행하면 끝
#
# 사용법 (SD를 PC에 꽂고, 드라이브 문자 확인 후):
#   powershell -ep bypass -File C:\Users\ehdgn\SOTA\zcutonpu\zcu102-coralnpu\09_remote\make_sd.ps1 E:
#
# SD에 들어가는 것:
#   \BOOT.BIN, \image.ub   ← 리눅스 부팅 (2019.1 프리빌트)
#   \board\...             ← NPU 추론 일체 (바이너리·비트스트림·테스트셋·스크립트)
# 이후 보드에 꽂고 전원 ON → PuTTY(COM3)에서:
#   root / root 로그인 →  sh /run/media/mmcblk0p1/board/run_demo.sh 8

param([Parameter(Mandatory=$true)][string]$Drive)

$R = Split-Path -Parent $MyInvocation.MyCommand.Path   # 폴더 이름 무관
$D = $Drive.TrimEnd('\')

if (!(Test-Path "$D\")) { Write-Error "드라이브 $D 없음 — SD가 꽂혔는지, 문자가 맞는지 확인"; exit 1 }

# 안전핀: 기존 베어메탈 BOOT.bin이 SD에 있으면 이름 바꿔 보존
if (Test-Path "$D\BOOT.bin") {
  Copy-Item "$D\BOOT.bin" "$D\BOOT_baremetal_mnist.bak" -Force
  Write-Host "기존 BOOT.bin -> BOOT_baremetal_mnist.bak 로 보존"
}

Write-Host "1) 리눅스 부팅 파일 복사 (image.ub 79MB — 좀 걸림)"
Copy-Item "$R\sd\BOOT.BIN"  "$D\BOOT.BIN"  -Force
Copy-Item "$R\sd\image.ub"  "$D\image.ub"  -Force

Write-Host "2) board 폴더 복사"
New-Item -ItemType Directory -Force -Path "$D\board" | Out-Null
$files = @("coral_probe","coral_mnist8","coral_mnist28","run_demo.sh",
           "coral_base.bin","coral_base_swapped.bin",
           "t10k_x.bin","t10k_y.bin","bit2bin.py")
foreach ($f in $files) { Copy-Item "$R\board\$f" "$D\board\" -Force }

Write-Host "`n=== SD 내용 확인 ==="
Get-ChildItem $D -Recurse -File | Select FullName, @{n='MB';e={[math]::Round($_.Length/1MB,1)}}

Write-Host "`n완료. 안전 제거 후 보드에 꽂고 전원 ON."
Write-Host "SW6는 그대로 (1-ON / 2,3,4-OFF)."
Write-Host "PuTTY COM3 115200 -> root/root -> sh /run/media/mmcblk0p1/board/run_demo.sh 8"
