#!/bin/sh
# 보드(리눅스)에서 실행 — 비트스트림 로드 + 클럭 50MHz + MNIST 추론
# 사용: sh run_demo.sh [8|28|full]   (기본 28)
# 전제: 이 폴더(board/)가 보드에서 접근 가능 (SD FAT 파티션 또는 scp)
cd "$(dirname "$0")"
MODE="${1:-28}"

# 보드 IP 표시 (SSH/CI 연결용 — 메모해둘 것)
echo "== board IP =="
ifconfig eth0 2>/dev/null | grep "inet " || ip addr show eth0 2>/dev/null | grep "inet "

# FAT에서는 실행권한이 없으므로 /tmp로 복사해 실행
cp coral_probe coral_mnist8 coral_mnist28 /tmp/ 2>/dev/null
chmod +x /tmp/coral_probe /tmp/coral_mnist8 /tmp/coral_mnist28

# 1) 비트스트림 — 기본은 SKIP (BOOT.BIN의 FSBL이 이미 로드함).
#    런타임 로드를 시도하려면 LOAD_BIT=1 sh run_demo.sh
#    ※ 주의: PS(HPM0) 설정이 안 맞는 BOOT.BIN이면 PL 접근 시 SError->커널패닉
if [ -n "$LOAD_BIT" ]; then
  BIN=coral_base.bin
  [ -f "$BIN" ] || { echo "coral_base.bin 없음 — PC에서 bit2bin.py 먼저"; exit 2; }
  if command -v fpgautil >/dev/null; then
    fpgautil -b "$BIN" || { echo "실패 → swapped 재시도"; fpgautil -b coral_base_swapped.bin || exit 3; }
  else
    mkdir -p /lib/firmware && cp "$BIN" /lib/firmware/
    echo 0 > /sys/class/fpga_manager/fpga0/flags
    echo "$BIN" > /sys/class/fpga_manager/fpga0/firmware || exit 3
    sleep 1; cat /sys/class/fpga_manager/fpga0/state
  fi
fi

# 2) PL 클럭 확인만 (설정은 FSBL이 이미 함).
#    ※ PMUFW 동작 중에는 CRL_APB 직접 쓰기가 막혀 SError가 날 수 있어 읽기만 한다.
#       강제로 바꾸려면 SET_CLK=1 (권장하지 않음)
if [ -n "$SET_CLK" ]; then /tmp/coral_probe --clk50; else /tmp/coral_probe --clkshow; fi

# 3) PL 상태 확인 후 AXI 창 탐침
st=$(cat /sys/class/fpga_manager/fpga0/state 2>/dev/null)
echo "== fpga0 state: ${st:-unknown} =="
/tmp/coral_probe || echo "(탐침 경고 — 위 출력 확인)"

# 4) 추론
case "$MODE" in
  8)    /tmp/coral_mnist8 ;;
  full) /tmp/coral_mnist28 --full "$(pwd)" ;;
  *)    /tmp/coral_mnist28 ;;
esac
