# =============================================================================
#  run_overnight.ps1 — 밤새 완전한 Coral NPU 비트스트림 + BOOT.bin 만들기
# =============================================================================
#  실행 (PowerShell, 퇴근 전 한 줄):
#    powershell -ep bypass -File C:\Users\ehdgn\SOTA\zcutonpu\zcu102-nvdla\09_remote\run_overnight.ps1
#
#  하는 일
#    1) Vivado 로 VmeCoreMiniAxi(스칼라+벡터+행렬) 합성/구현/비트스트림  (수 시간)
#    2) bootgen 으로 BOOT.bin 재조립 (기존 fsbl.elf + 새 비트스트림 + 기존 앱)
#    3) 결과를 09_remote\sd_full\ 에 모아두고 STATUS.md 에 결과 기록
#
#  안전장치
#    - 기존 프로젝트/BOOT.bin 은 절대 건드리지 않는다.
#    - 실패해도 지금 동작하는 28x28 데모 SD는 그대로 쓸 수 있다.
#
#  왜 앱(coral_app.elf)을 다시 안 빌드해도 되나
#    PS 설정과 주소맵(0x5_0000_0000)이 기존과 동일하고, MNIST 커널은 rv32im
#    스칼라 코드다. 완전체 코어는 그 상위집합이라 같은 코드가 그대로 돈다.
#    즉 바뀌는 것은 PL 비트스트림 하나뿐이다.
# =============================================================================

$ErrorActionPreference = "Continue"

$ROOT   = "C:\Users\ehdgn\SOTA\zcutonpu"
$REPO   = "$ROOT\zcu102-nvdla"
$OUT    = "$REPO\09_remote\sd_full"
$LOG    = "$OUT\overnight.log"
$STATUS = "$OUT\STATUS.md"

$VIVADO  = "C:\AMDDesignTools\2026.1\Vivado\bin\vivado.bat"
$BOOTGEN = "C:\AMDDesignTools\2026.1\Vitis\bin\bootgen.bat"

$TCL     = "$REPO\01_hw\build_full_vme.tcl"
$PROJ    = "$ROOT\01_hw\coral_zcu102_vme"
$BIT     = "$PROJ\coral_zcu102_vme.runs\impl_1\coralnpu_vme_wrapper.bit"
$FSBL    = "$ROOT\01_hw\vitis_ws\coral_platform\zynqmp_fsbl\build\fsbl.elf"
$APP     = "$ROOT\01_hw\vitis_ws\coral_app\build\coral_app.elf"

New-Item -ItemType Directory -Force -Path $OUT | Out-Null
"" | Set-Content $LOG

function Say($m) {
    $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m
    Write-Host $line
    Add-Content -Path $LOG -Value $line
}

Say "=============================================="
Say " 완전한 Coral NPU 야간 빌드 시작"
Say "=============================================="

# ---------------------------------------------------------------- 사전 점검
$missing = @()
foreach ($p in @($VIVADO, $BOOTGEN, $TCL, $FSBL, $APP)) {
    if (!(Test-Path $p)) { $missing += $p }
}
if ($missing.Count -gt 0) {
    Say "중단 — 다음 파일을 못 찾음:"
    foreach ($m in $missing) { Say "   $m" }
    Say "경로를 고치고 다시 실행하세요."
    exit 1
}
Say "사전 점검 통과 (Vivado / bootgen / Tcl / fsbl.elf / coral_app.elf 모두 존재)"

# ---------------------------------------------------------------- 1. Vivado
Say "[1/3] Vivado 빌드 시작 — 합성+구현+비트스트림 (수 시간)"
$t0 = Get-Date
& $VIVADO -mode batch -notrace -source $TCL 2>&1 | Tee-Object -FilePath "$OUT\vivado_full.log"
$mins = [int]((Get-Date) - $t0).TotalMinutes
Say "[1/3] Vivado 종료 (${mins}분). 로그: $OUT\vivado_full.log"

if (!(Test-Path $BIT)) {
    Say "비트스트림이 생성되지 않았습니다: $BIT"
    Say "vivado_full.log 에서 ERROR 를 확인하세요."
    @"
# 야간 빌드 결과 — 실패 (비트스트림 없음)

Vivado 단계에서 멈췄습니다. 'vivado_full.log' 의 ERROR 부분을 Claude에게 보여주세요.

**아침에 할 일: 없음.** 기존 SD(28x28 베어메탈 데모)를 그대로 쓰면 됩니다.
발표용 데모는 안전합니다.
"@ | Set-Content -Encoding UTF8 $STATUS
    exit 1
}
Say "비트스트림 확인: $BIT"
Copy-Item $BIT "$OUT\coralnpu_vme_wrapper.bit" -Force

# 타이밍 결과 뽑아두기
$wns = "미확인"
$m = Select-String -Path "$OUT\vivado_full.log" -Pattern "WNS : (\S+) ns" | Select-Object -Last 1
if ($m) { $wns = $m.Matches[0].Groups[1].Value; Say "타이밍 WNS = $wns ns" }

# ---------------------------------------------------------------- 2. bootgen
Say "[2/3] BOOT.bin 재조립 (bootgen — 라이선스 불필요)"
$bif = "$OUT\coral_vme.bif"
@"
//arch = zynqmp; split = false; format = BIN
the_ROM_image:
{
	[bootloader, destination_cpu = a53-0]$FSBL
	[destination_device = pl]$OUT\coralnpu_vme_wrapper.bit
	[destination_cpu = a53-0, exception_level = el-3]$APP
}
"@ | Set-Content -Encoding ASCII $bif

Push-Location $OUT
& $BOOTGEN -arch zynqmp -image $bif -o "$OUT\BOOT.bin" -w on 2>&1 | Tee-Object -FilePath "$OUT\bootgen.log"
$rc = $LASTEXITCODE
Pop-Location

if ($rc -ne 0 -or !(Test-Path "$OUT\BOOT.bin")) {
    Say "bootgen 실패 (코드 $rc) — bootgen.log 확인"
    exit 1
}
$mb = [math]::Round((Get-Item "$OUT\BOOT.bin").Length / 1MB, 1)
Say "[2/3] BOOT.bin 생성 완료 (${mb}MB)"

# ---------------------------------------------------------------- 3. 정리
Say "[3/3] 결과 정리"
@"
# 야간 빌드 결과 — 성공

| 항목 | 값 |
|---|---|
| 코어 | VmeCoreMiniAxi (스칼라 + 벡터/SIMD + 행렬) |
| PL 클럭 | 33 MHz |
| 타이밍 WNS | $wns ns |
| BOOT.bin | ${mb} MB |
| 만든 시각 | $(Get-Date -Format "yyyy-MM-dd HH:mm") |

## 아침에 할 일 (2분)

1. SD를 PC에 꽂고 드라이브 문자 확인 (예: G:)
2. 한 줄 실행:

   powershell -ep bypass -File C:\Users\ehdgn\SOTA\zcutonpu\zcu102-nvdla\09_remote\make_sd_full.ps1 G:

3. SD를 보드에 꽂고 전원 ON (SW6: 1-ON / 2,3,4-OFF)
4. PuTTY COM3 115200 — 기존과 똑같은 MNIST 출력이 나오면 성공.
   **이번엔 그 계산이 완전한 Coral NPU에서 돌아간 것입니다.**

## 되돌리기

make_sd_full.ps1 이 기존 BOOT.bin 을 'BOOT_scalar.bak' 로 보존합니다.
문제가 생기면 그 파일을 'BOOT.bin' 으로 되돌리면 즉시 원래 데모로 복귀합니다.
"@ | Set-Content -Encoding UTF8 $STATUS

Say "=============================================="
Say " 완료. 결과: $OUT"
Say " 아침 절차: $STATUS"
Say "=============================================="
