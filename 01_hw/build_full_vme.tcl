# =============================================================================
#  build_full_vme.tcl — 완전한 Coral NPU(VmeCoreMiniAxi) 비트스트림 생성
# =============================================================================
#  rebuild_project.tcl 과 구조가 같고, 코어만 완전체로 바뀐 판이다.
#  이 파일은 보통 run_overnight.ps1 이 자동으로 호출한다.
#
#  기존 프로젝트(coral_zcu102)는 건드리지 않는다. 새 폴더에 따로 만든다.
#  즉 실패해도 지금 동작하는 데모는 그대로 남는다.
# =============================================================================

set RTL_FULL  "C:/Users/ehdgn/SOTA/zcutonpu/01_hw/coral_rtl_full"
set RTL_REPO  "C:/Users/ehdgn/SOTA/zcutonpu/zcu102-nvdla/01_hw"
set PROJ_DIR  "C:/Users/ehdgn/SOTA/zcutonpu/01_hw/coral_zcu102_vme"
set PROJ_NAME "coral_zcu102_vme"
set PART      "xczu9eg-ffvb1156-2-e"
set BOARD     "xilinx.com:zcu102:part0:3.4"
set BD_NAME   "coralnpu_vme"
set JOBS      10

# 클럭: 기존 스칼라 코어는 50MHz(WNS +0.462)였지만 완전체는 훨씬 크다.
# 첫 시도부터 타이밍으로 밤을 날리지 않도록 33MHz로 보수적으로 잡는다.
# (추론 속도는 느려지지만 MNIST 한 장은 여전히 수 ms 수준)
if {![info exists PL_FREQ]} { set PL_FREQ 33 }

puts "=============================================================="
puts " 완전한 Coral NPU (VmeCoreMiniAxi) 빌드"
puts "   PART    : $PART     PL_CLK0 = ${PL_FREQ}MHz"
puts "   PROJ    : $PROJ_DIR"
puts "=============================================================="

create_project $PROJ_NAME $PROJ_DIR -part $PART -force
if {[llength [get_board_parts -quiet $BOARD]] > 0} {
    set_property board_part $BOARD [current_project]
    puts "\[OK\] 보드 파트 적용"
} else {
    puts "\[경고\] ZCU102 보드 파일 없음 — PS 프리셋이 안 잡힐 수 있음"
}

# --- RTL: 완전체 코어 + 전용 wrapper ----------------------------------------
#  ★ 기존 CoreMiniAxi.sv / coral_axi_wrapper.sv 는 넣지 않는다.
#    두 wrapper는 이름이 다르지만, 굳이 함께 넣어 합성 시간을 늘릴 이유가 없다.
add_files -norecurse [list \
    "$RTL_FULL/VmeCoreMiniAxi.sv" \
    "$RTL_REPO/coral_axi_wrapper_vme.sv" \
]
set_property file_type SystemVerilog [get_files *.sv]
update_compile_order -fileset sources_1

# --- Verilog define ----------------------------------------------------------
#  SYNTHESIS   : Sram이 BRAM 추론 경로를 탐 (없으면 합성 불가)
#  FPGA_XILINX : ClockGate가 BUFGCE(ULTRASCALE_PLUS)를 씀
#  ZVE32F_ON   : 벡터 부동소수점
#  ZVT_ON      : 행렬(VME) 확장 — 없으면 완전체가 아님
#  VLEN_128    : ★ 벡터 레지스터 폭. 없으면 `VLEN 매크로가 정의되지 않아
#                561곳에서 미정의 매크로가 되고 구문 오류가 난다.
#                (2026-08-13 첫 시도에서 실제로 이것 때문에 실패)
set DEFS {SYNTHESIS FPGA_XILINX ZVE32F_ON ZVT_ON VLEN_128}
set_property verilog_define $DEFS [get_filesets sources_1]
set_property -name {STEPS.SYNTH_DESIGN.ARGS.MORE OPTIONS} \
             -value {-verilog_define SYNTHESIS -verilog_define FPGA_XILINX -verilog_define ZVE32F_ON -verilog_define ZVT_ON -verilog_define VLEN_128} \
             -objects [get_runs synth_1]
puts "\[OK\] define = $DEFS"

# --- Block Design ------------------------------------------------------------
create_bd_design $BD_NAME

create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ultra_ps_e_0
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e \
    -config {apply_board_preset "1"} [get_bd_cells zynq_ultra_ps_e_0]
set_property -dict [list \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ $PL_FREQ \
    CONFIG.PSU__USE__M_AXI_GP0     {1} \
    CONFIG.PSU__MAXIGP0__DATA_WIDTH {128} \
    CONFIG.PSU__USE__M_AXI_GP1     {1} \
    CONFIG.PSU__MAXIGP1__DATA_WIDTH {128} \
    CONFIG.PSU__USE__S_AXI_GP2     {1} \
    CONFIG.PSU__SAXIGP2__DATA_WIDTH {128} \
] [get_bd_cells zynq_ultra_ps_e_0]
#  ★ HPM1 을 반드시 켠다 (2026-08-13에 밝혀진 것)
#    ZynqMP에서 M_AXI_HPM0_FPD 의 주소창은 0xA000_0000 / 0x4_0000_0000 /
#    0x10_0000_0000 이고, 0x5_0000_0000 은 HPM1_FPD 의 창이다.
#    소프트웨어(main_mnist28.c)가 CB=0x5_0000_0000 을 쓰므로 HPM1이 없으면
#    "must fit an available aperture" 에러로 주소 배정 자체가 실패한다.
#    실제 동작 중인 기존 설계(coralnpu.bd)도 HPM0+HPM1 둘 다 쓰고 있었다.

create_bd_cell -type module -reference coral_axi_wrapper_vme coral_axi_wrapper_0

# 제어 경로 SmartConnect: 슬레이브 포트 2개 (HPM1 -> S00, HPM0 -> S01)
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect axi_smc_ctrl
set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {1}] [get_bd_cells axi_smc_ctrl]
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect axi_smc_mem
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] [get_bd_cells axi_smc_mem]
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset rst_ps_pl

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant const_boot_addr
set_property -dict [list CONFIG.CONST_WIDTH {32} CONFIG.CONST_VAL {0}] [get_bd_cells const_boot_addr]
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant const_irq
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {0}] [get_bd_cells const_irq]

connect_bd_intf_net [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM1_FPD] [get_bd_intf_pins axi_smc_ctrl/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM0_FPD] [get_bd_intf_pins axi_smc_ctrl/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_smc_ctrl/M00_AXI]             [get_bd_intf_pins coral_axi_wrapper_0/s_axi]
connect_bd_intf_net [get_bd_intf_pins coral_axi_wrapper_0/m_axi]        [get_bd_intf_pins axi_smc_mem/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_smc_mem/M00_AXI]              [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HP0_FPD]

connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
    [get_bd_pins zynq_ultra_ps_e_0/maxihpm0_fpd_aclk] \
    [get_bd_pins zynq_ultra_ps_e_0/maxihpm1_fpd_aclk] \
    [get_bd_pins zynq_ultra_ps_e_0/saxihp0_fpd_aclk] \
    [get_bd_pins coral_axi_wrapper_0/aclk] \
    [get_bd_pins axi_smc_ctrl/aclk] \
    [get_bd_pins axi_smc_mem/aclk] \
    [get_bd_pins rst_ps_pl/slowest_sync_clk]

connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0] [get_bd_pins rst_ps_pl/ext_reset_in]
connect_bd_net [get_bd_pins rst_ps_pl/peripheral_aresetn] \
    [get_bd_pins coral_axi_wrapper_0/aresetn] \
    [get_bd_pins axi_smc_ctrl/aresetn] \
    [get_bd_pins axi_smc_mem/aresetn]

connect_bd_net [get_bd_pins const_boot_addr/dout] [get_bd_pins coral_axi_wrapper_0/boot_addr]
connect_bd_net [get_bd_pins const_irq/dout]       [get_bd_pins coral_axi_wrapper_0/irq]

# --- 주소: 0x5_0000_0000 (소프트웨어의 CB 매크로와 반드시 일치) --------------
#  ★ coral_axi_wrapper_0 의 세그먼트를 전부 돌며 0x5_0000_0000 으로 강제하면
#    안 된다. m_axi(마스터) 쪽 세그먼트까지 같은 주소로 밀려 서로 겹치고
#    validate_bd_design 이 실패한다 (2026-08-13 첫 시도의 두 번째 원인).
#    m_axi 쪽은 자동 배정값(DDR 0x0 / QSPI 0xC000_0000 등)을 그대로 두고,
#    PS->Coral 슬레이브 창 하나만 옮긴다.
assign_bd_address
set seg [get_bd_addr_segs -quiet coral_axi_wrapper_0/s_axi/reg0]
if {[llength $seg] > 0} {
    assign_bd_address -force \
        -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] \
        $seg -offset 0x0000000500000000 -range 4G
    puts "\[OK\] coral s_axi = 0x5_0000_0000 (기존 BOOT.bin 소프트웨어와 동일)"
} else {
    puts "\[경고\] coral s_axi 세그먼트를 못 찾음 — Address Editor 확인 필요"
}

validate_bd_design
regenerate_bd_layout
save_bd_design

set bd_file [get_files ${BD_NAME}.bd]
make_wrapper -files $bd_file -top -force
set wv [glob -nocomplain "${PROJ_DIR}/${PROJ_NAME}.gen/sources_1/bd/${BD_NAME}/hdl/${BD_NAME}_wrapper.v"]
if {$wv eq ""} { set wv [glob -nocomplain "${PROJ_DIR}/${PROJ_NAME}.srcs/sources_1/bd/${BD_NAME}/hdl/${BD_NAME}_wrapper.v"] }
if {$wv ne ""} { add_files -norecurse $wv }

# ★ Top은 반드시 BD 래퍼. coral_axi_wrapper_vme로 두면 IO 707개 초과 에러.
set_property top ${BD_NAME}_wrapper [get_filesets sources_1]
update_compile_order -fileset sources_1

puts "\n>>> 합성 시작 (완전체라 오래 걸림)"
launch_runs synth_1 -jobs $JOBS
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%"} { error "합성 실패" }
puts "\[OK\] 합성 완료"

puts "\n>>> 구현 + 비트스트림"
launch_runs impl_1 -to_step write_bitstream -jobs $JOBS
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} { error "구현/비트스트림 실패" }

open_run impl_1
set wns [get_property SLACK [get_timing_paths -delay_type max]]
report_utilization -file "${PROJ_DIR}/util_impl.txt"

set bit "${PROJ_DIR}/${PROJ_NAME}.runs/impl_1/${BD_NAME}_wrapper.bit"
set xsa "${PROJ_DIR}/${BD_NAME}_wrapper.xsa"
write_hw_platform -fixed -include_bit -force $xsa

puts "\n=============================================================="
puts " 완전체 빌드 완료"
puts "   비트스트림 : $bit"
puts "   XSA        : $xsa"
puts "   타이밍 WNS : $wns ns  (@ ${PL_FREQ}MHz)"
if {$wns < 0} { puts "   \[경고\] 타이밍 위반 — 동작이 불안정할 수 있음" }
puts "=============================================================="
