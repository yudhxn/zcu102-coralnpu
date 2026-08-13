# =============================================================================
#  make_sd_linux.ps1 — 리눅스 SD 만들기
# =============================================================================
#  사용법 (SD를 PC에 꽂고 드라이브 문자 확인 후):
#    powershell -ep bypass -File C:\Users\ehdgn\SOTA\zcutonpu\zcu102-nvdla\09_remote\make_sd_linux.ps1 G:
#
#  넣는 것
#    BOOT.BIN   : FSBL+PMU+ATF+u-boot+커널설정+★Coral 비트스트림 (전부 2026.1)
#    image.ub   : 리눅스 커널 + rootfs
#    boot.scr   : u-boot 부팅 스크립트
#    board\     : NPU 추론 바이너리 일체
#
#  ★ 비트스트림이 BOOT.BIN 안에 이미 들어있다.
#    부팅 시 FSBL이 PL에 올려주므로, 리눅스에서 fpga_manager로 따로
#    로드할 필요가 없다. (지난번 리눅스 시도 때의 위험요소 하나가 사라짐)
# =============================================================================

param([Parameter(Mandatory=$true)][string]$Drive)

$R   = Split-Path -Parent $MyInvocation.MyCommand.Path
$SRC = "$R\sd_linux"
$D   = $Drive.TrimEnd('\')

if (!(Test-Path "$D\")) { Write-Host "드라이브 $D 를 못 찾습니다." -ForegroundColor Red; exit 1 }
if (!(Test-Path "$SRC\BOOT.BIN")) { Write-Host "빌드 결과가 없습니다: $SRC" -ForegroundColor Red; exit 1 }

# 기존 베어메탈 BOOT.bin 보존
foreach ($n in @("BOOT.bin","BOOT.BIN")) {
    if (Test-Path "$D\$n") {
        if (!(Test-Path "$D\BOOT_baremetal.bak")) {
            Copy-Item "$D\$n" "$D\BOOT_baremetal.bak" -Force
            Write-Host "기존 $n -> BOOT_baremetal.bak 로 보존"
        }
    }
}

foreach ($f in @("BOOT.BIN","image.ub","boot.scr")) {
    Copy-Item "$SRC\$f" "$D\$f" -Force
    $mb = [math]::Round((Get-Item "$D\$f").Length / 1MB, 1)
    Write-Host "복사: $f (${mb}MB)"
}

if (Test-Path "$R\board") {
    Copy-Item "$R\board" "$D\" -Recurse -Force
    Write-Host "복사: board\ (NPU 추론 바이너리)"
}

Write-Host ""
Write-Host "=== 다음 ===" -ForegroundColor Green
Write-Host "1. SD 안전제거 -> 보드에 꽂기"
Write-Host "2. SW6 = 1-ON / 2,3,4-OFF, 랜선 연결, 전원 ON"
Write-Host "3. PuTTY COM3 115200"
Write-Host ""
Write-Host "부팅에 30~60초. 'zynqmp login:' 뜨면 root 입력 (비번 없음)"
Write-Host "그 다음:  ifconfig eth0     <- 보드 IP 확인"
Write-Host "          /run/media/mmcblk0p1/board/coral_probe"
