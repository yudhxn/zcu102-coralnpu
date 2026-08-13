# 퇴근 전 1회 실행 — 필요한 빌드 산출물을 저장소 폴더로 복사
# 실행: powershell -ep bypass -File C:\Users\ehdgn\SOTA\zcutonpu\zcu102-coralnpu\09_remote\copy_files.ps1
$R = Split-Path -Parent $MyInvocation.MyCommand.Path   # 폴더 이름 무관
New-Item -ItemType Directory -Force -Path "$R\sd","$R\bit","$R\fsbl" | Out-Null

# 1) 리눅스 부팅용 (2019.1 프리빌트: BOOT.BIN + image.ub)
Copy-Item "C:\Users\ehdgn\Downloads\2019.1-zcu102-release\2019.1-zcu102-release\BOOT.BIN" "$R\sd\" -Force
Copy-Item "C:\Users\ehdgn\Downloads\2019.1-zcu102-release\2019.1-zcu102-release\image.ub" "$R\sd\" -Force -EA SilentlyContinue
Get-ChildItem "C:\Users\ehdgn\Downloads\2019.1-zcu102-release\2019.1-zcu102-release\" | Select Name

# 2) Coral 비트스트림 (기존 검증본 + highmem)
Copy-Item "C:\Users\ehdgn\SOTA\zcutonpu\01_hw\coral_zcu102\coral_zcu102.runs\impl_1\coralnpu_wrapper.bit" "$R\bit\coral_base.bit" -Force
Copy-Item "C:\Users\ehdgn\SOTA\zcutonpu\01_hw\coral_zcu102_highmem\coralnpu_wrapper.xsa" "$R\bit\coral_highmem.xsa" -Force

# 3) FSBL (나중에 bootgen용)
Copy-Item "C:\Users\ehdgn\SOTA\zcutonpu\01_hw\vitis_ws\coral_platform\export\coral_platform\sw\boot\fsbl.elf" "$R\fsbl\" -Force -EA SilentlyContinue
if (!(Test-Path "$R\fsbl\fsbl.elf")) {
  Get-ChildItem "C:\Users\ehdgn\SOTA\zcutonpu\01_hw\vitis_ws" -Recurse -Filter "fsbl*.elf" -EA SilentlyContinue | Select -First 1 | Copy-Item -Destination "$R\fsbl\fsbl.elf" -Force
}

Write-Host "`n=== 복사 결과 ==="
Get-ChildItem $R -Recurse -File | Select FullName, @{n='MB';e={[math]::Round($_.Length/1MB,1)}}
