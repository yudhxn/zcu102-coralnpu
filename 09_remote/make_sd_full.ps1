# =============================================================================
#  make_sd_full.ps1 — 아침에 SD 한 장 갈아끼우기 (2분)
# =============================================================================
#  사용법 (SD를 PC에 꽂고 드라이브 문자 확인 후):
#    powershell -ep bypass -File C:\Users\ehdgn\SOTA\zcutonpu\zcu102-nvdla\09_remote\make_sd_full.ps1 G:
#
#  하는 일
#    1) SD의 기존 BOOT.bin 을 BOOT_scalar.bak 으로 보존 (되돌리기용)
#    2) 밤새 만든 완전체 BOOT.bin 을 SD 루트에 복사
#    3) image.ub 가 남아 있으면 삭제 (베어메탈 부팅을 방해함)
#
#  밤 빌드가 실패했으면 아무것도 바꾸지 않고 안내만 하고 끝난다.
# =============================================================================

param([Parameter(Mandatory=$true)][string]$Drive)

$SRC = "C:\Users\ehdgn\SOTA\zcutonpu\zcu102-nvdla\09_remote\sd_full\BOOT.bin"
$D   = $Drive.TrimEnd('\')

if (!(Test-Path "$D\")) {
    Write-Host "드라이브 $D 를 못 찾습니다. SD가 꽂혔는지, 문자가 맞는지 확인하세요." -ForegroundColor Red
    exit 1
}

if (!(Test-Path $SRC)) {
    Write-Host ""
    Write-Host "밤 빌드 결과물이 없습니다." -ForegroundColor Yellow
    Write-Host "  기대 위치: $SRC"
    Write-Host ""
    Write-Host "SD는 건드리지 않았습니다. 기존 28x28 데모가 그대로 들어있으니"
    Write-Host "발표/시연은 지금 상태로 가능합니다."
    Write-Host "원인은 sd_full\overnight.log 와 vivado_full.log 에 있습니다."
    exit 2
}

# 1. 기존 것 보존
if (Test-Path "$D\BOOT.bin") {
    Copy-Item "$D\BOOT.bin" "$D\BOOT_scalar.bak" -Force
    Write-Host "기존 BOOT.bin -> BOOT_scalar.bak 로 보존했습니다 (되돌리기용)"
}

# 2. 완전체 복사
Copy-Item $SRC "$D\BOOT.bin" -Force
$mb = [math]::Round((Get-Item "$D\BOOT.bin").Length / 1MB, 1)
Write-Host "완전체 BOOT.bin 복사 완료 (${mb}MB)"

# 3. 리눅스 잔재 제거
if (Test-Path "$D\image.ub") {
    Remove-Item "$D\image.ub" -Force
    Write-Host "image.ub 삭제 (베어메탈 부팅 방해 요소)"
}

Write-Host ""
Write-Host "=== 다음 ===" -ForegroundColor Green
Write-Host "1. SD를 안전하게 제거 -> 보드에 꽂기"
Write-Host "2. SW6 = 1-ON / 2,3,4-OFF 확인 후 전원 ON"
Write-Host "3. PuTTY COM3, 115200 8N1"
Write-Host ""
Write-Host "기존과 똑같은 MNIST 화면이 나오면 성공입니다."
Write-Host "다른 점은 그 계산이 이제 완전한 Coral NPU(스칼라+벡터+행렬)에서"
Write-Host "돌아간다는 것입니다."
Write-Host ""
Write-Host "이상하면: SD의 BOOT_scalar.bak 을 BOOT.bin 으로 이름 바꾸면 즉시 복구됩니다."
