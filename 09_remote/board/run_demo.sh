#!/bin/sh
# 보드(PetaLinux 2026.1)에서 실행 — Coral NPU MNIST 추론
#
# 사용: sh run_demo.sh [28|full|8]     (기본 28)
#
# 전제 (2026-08-13 기준, 모두 이미지에 구워져 있음)
#   · BOOT.BIN 의 FSBL 이 부팅 시 비트스트림을 PL 에 올린다 → 런타임 로드 불필요
#   · 커널 부팅옵션 clk_ignore_unused / pd_ignore_unused 로 PL 클럭·전원 유지
#     (이게 없으면 NPU 접근 순간 보드가 멈추거나 SError 패닉)
#
# ★ coral_probe (인자 없이)는 CSR +0x30008 을 읽어 패닉을 유발하므로 쓰지 않는다.
#   상태 확인이 필요하면 rdprobe (읽기 전용) 를 쓸 것.

cd "$(dirname "$0")"
MODE="${1:-28}"

echo "== board =="
ip -br addr show end0 2>/dev/null || ip -br addr 2>/dev/null | grep -v "^lo"
echo "== fpga =="
cat /sys/class/fpga_manager/fpga0/state 2>/dev/null
echo "== cmdline =="
cat /proc/cmdline

# FAT 파티션은 실행권한이 없으므로 /tmp 로 복사해 실행
cp -f coral_mnist28 coral_mnist8 rdprobe /tmp/ 2>/dev/null
chmod +x /tmp/coral_mnist28 /tmp/coral_mnist8 /tmp/rdprobe 2>/dev/null

echo "== npu window =="
/tmp/rdprobe 0x500000000

echo "== inference =="
case "$MODE" in
  8)    /tmp/coral_mnist8 ;;
  full) /tmp/coral_mnist28 --full "$(pwd)" ;;
  *)    /tmp/coral_mnist28 ;;
esac
