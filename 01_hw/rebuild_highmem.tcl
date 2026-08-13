# =============================================================================
#  rebuild_highmem.tcl  —  Coral NPU highmem (ITCM/DTCM 각 1MB) 합성
# =============================================================================
#
#  목적
#    더 큰 모델(28x28 CNN 등)을 올리기 위해 온칩 메모리를 키운 버전을 합성한다.
#
#  base 대비 차이 (2026-08-12 RTL 실측)
#    ITCM   8KB @0x0      ->  1MB @0x00000000   (128배)
#    DTCM  32KB @0x10000  ->  1MB @0x00100000   ( 32배)
#    32KB Sram 뱅크 64개가 0x0~0x1FFFFF 에 빈틈없이 배치됨 (총 2MB)
#    BRAM 약 455개 소요 (ZU9EG 912개의 절반)
#
#  ※ DTCM 주소가 바뀌므로 소프트웨어(main_*.c)의 오프셋도 함께 수정해야 한다.
#
#  실행 방법 (Vivado Tcl Console 또는 배치 모드)
#    vivado -mode batch -source rebuild_project.tcl
#      또는
#    Vivado GUI -> Tools -> Run Tcl Script...
#
#  실행 전 확인
#    1) Vivado 라이선스 등록 완료 (xczu9eg는 무료 에디션으로 안 됨)
#    2) ZCU102 보드 파일 설치됨
#       Tools -> Vivado Store -> Boards -> ZCU102 검색 후 설치
#    3) 아래 RTL_DIR / PROJ_DIR 경로 확인
#
#  소요 시간: 합성+구현+비트스트림까지 대략 40분 ~ 1시간 30분
# =============================================================================

# ---------------------------------------------------------------------------
# 0. 사용자 설정 — 경로만 확인하면 됩니다
# ---------------------------------------------------------------------------
set RTL_DIR   "C:/Users/ehdgn/SOTA/zcutonpu/zcu102-coralnpu/01_hw"
set PROJ_DIR  "C:/Users/ehdgn/SOTA/zcutonpu/01_hw/coral_zcu102_hm"
set PROJ_NAME "coral_zcu102_hm"
set PART      "xczu9eg-ffvb1156-2-e"
set BOARD     "xilinx.com:zcu102:part0:3.4"
set BD_NAME   "coralnpu"
set PL_FREQ   50
set JOBS      8

puts "=============================================================="
puts " Coral NPU / ZCU102  프로젝트 재건 시작"
puts "   RTL  : $RTL_DIR"
puts "   PROJ : $PROJ_DIR"
puts "   PART : $PART   /  PL_CLK0 = ${PL_FREQ}MHz"
puts "=============================================================="

# ---------------------------------------------------------------------------
# 1. 프로젝트 생성
# ---------------------------------------------------------------------------
create_project $PROJ_NAME $PROJ_DIR -part $PART -force

# 보드 파일이 설치돼 있으면 PS 프리셋(DDR4, UART, 클럭)이 자동 적용된다.
# 설치 안 돼 있으면 경고만 뜨고 넘어가지만, PS 설정을 손으로 해야 하므로
# 반드시 Vivado Store에서 ZCU102 보드 파일을 먼저 설치할 것.
if {[llength [get_board_parts -quiet $BOARD]] > 0} {
    set_property board_part $BOARD [current_project]
    puts "\[OK\] 보드 파트 적용: $BOARD"
} else {
    puts "\[경고\] ZCU102 보드 파일이 없습니다."
    puts "        Tools -> Vivado Store -> Boards 에서 설치 후 다시 실행하세요."
}

# ---------------------------------------------------------------------------
# 2. RTL 소스 추가
#
#  ★ 함정 (LOG.md 2026-07-20 기록)
#    CoreMiniAxi.sv 는 Chisel이 뱉은 자기완결형 파일로,
#    내부에 RstSync / ClockGate / Sram 모듈이 이미 들어있다.
#    (실제 확인: 19146행 CoreMiniAxi, 24943행 RstSync,
#                25010행 ClockGate, 36392행 Sram)
#    따라서 ClockGate.sv, RstSync.sv 를 따로 추가하면
#    "module redefinition" 에러가 난다. 절대 추가하지 말 것.
#
#    ★ 이 스크립트는 highmem 전용이다.
#      coral_axi_wrapper.sv 와 coral_axi_wrapper_highmem.sv 는 **모듈명이 둘 다
#      coral_axi_wrapper** 라서 같이 넣으면 중복 정의 에러가 난다. 하나만 넣을 것.
# ---------------------------------------------------------------------------
add_files -norecurse [list \
    "$RTL_DIR/CoreMiniHighmemAxi.sv" \
    "$RTL_DIR/coral_axi_wrapper_highmem.sv" \
]
set_property file_type SystemVerilog [get_files *.sv]
update_compile_order -fileset sources_1
puts "\[OK\] highmem RTL 2개 추가 (ITCM 1MB @0x0 / DTCM 1MB @0x100000)"

# ---------------------------------------------------------------------------
# 3. Verilog define: SYNTHESIS
#
#  ★ 함정 (LOG.md 2026-07-20, 160행)
#    이 define이 없으면 CoreMiniAxi 내부 메모리가
#    DPI-C(시뮬레이션 전용) 경로로 잡혀 합성이 되지 않는다.
#    SYNTHESIS 를 줘야 BRAM 추론 경로가 선택된다. 가장 중요한 설정.
# ---------------------------------------------------------------------------
set_property verilog_define {SYNTHESIS} [get_filesets sources_1]
set_property -name {STEPS.SYNTH_DESIGN.ARGS.MORE OPTIONS} \
             -value {-verilog_define SYNTHESIS} \
             -objects [get_runs synth_1]
puts "\[OK\] SYNTHESIS define 설정 (BRAM 추론 경로 선택)"

# ---------------------------------------------------------------------------
# 4. Block Design 생성
# ---------------------------------------------------------------------------
create_bd_design $BD_NAME

# --- 4-1. Zynq UltraScale+ PS -----------------------------------------------
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ultra_ps_e_0
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e \
    -config {apply_board_preset "1"} [get_bd_cells zynq_ultra_ps_e_0]

# PS 설정
#   PL0_REF_CTRL 50MHz : LOG.md 2026-07-23 — 100MHz는 타이밍 위반(WNS -2.385)
#                        50MHz로 낮춰 WNS +0.462 확보
#   M_AXI_HPM0_FPD(GP0) : PS -> Coral 제어 경로, 128bit
#   S_AXI_HP0_FPD(GP2)  : Coral -> DDR 접근 경로, 128bit
set_property -dict [list \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ $PL_FREQ \
    CONFIG.PSU__USE__M_AXI_GP0     {1} \
    CONFIG.PSU__MAXIGP0__DATA_WIDTH {128} \
    CONFIG.PSU__USE__M_AXI_GP1     {0} \
    CONFIG.PSU__USE__S_AXI_GP2     {1} \
    CONFIG.PSU__SAXIGP2__DATA_WIDTH {128} \
] [get_bd_cells zynq_ultra_ps_e_0]
puts "\[OK\] PS 설정: PL_CLK0=${PL_FREQ}MHz, HPM0(128b), HP0(128b)"

# --- 4-2. Coral wrapper (모듈 참조) ------------------------------------------
#  ★ 함정 (LOG.md 2026-07-21)
#    GUI의 "Add Module"은 "Module references are still updating" 에서
#    무한 대기하는 경우가 있다. Tcl로 직접 생성하면 우회된다.
create_bd_cell -type module -reference coral_axi_wrapper coral_axi_wrapper_0
puts "\[OK\] coral_axi_wrapper_0 생성 (Tcl 직접 생성으로 GUI 버그 우회)"

# --- 4-3. SmartConnect 2개 + Processor System Reset --------------------------
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect axi_smc_ctrl
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] \
    [get_bd_cells axi_smc_ctrl]

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect axi_smc_mem
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] \
    [get_bd_cells axi_smc_mem]

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset rst_ps_pl

# --- 4-4. 상수 (boot_addr = 0, irq = 0) --------------------------------------
#  LOG.md 2026-07-21: irq 자동 인터럽트 연결은 실패했으므로 Constant로 고정
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant const_boot_addr
set_property -dict [list CONFIG.CONST_WIDTH {32} CONFIG.CONST_VAL {0}] \
    [get_bd_cells const_boot_addr]

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant const_irq
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {0}] \
    [get_bd_cells const_irq]

# ---------------------------------------------------------------------------
# 5. 연결
# ---------------------------------------------------------------------------
# --- 5-1. AXI 경로 -----------------------------------------------------------
#   PS M_AXI_HPM0_FPD -> smc_ctrl -> coral s_axi      (PS가 Coral 제어)
#   coral m_axi       -> smc_mem  -> PS S_AXI_HP0_FPD (Coral이 DDR 접근)
connect_bd_intf_net [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM0_FPD] \
                    [get_bd_intf_pins axi_smc_ctrl/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_smc_ctrl/M00_AXI] \
                    [get_bd_intf_pins coral_axi_wrapper_0/s_axi]
connect_bd_intf_net [get_bd_intf_pins coral_axi_wrapper_0/m_axi] \
                    [get_bd_intf_pins axi_smc_mem/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_smc_mem/M00_AXI] \
                    [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HP0_FPD]

# --- 5-2. 클럭 (전부 pl_clk0 = 50MHz 하나로) ---------------------------------
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
    [get_bd_pins zynq_ultra_ps_e_0/maxihpm0_fpd_aclk] \
    [get_bd_pins zynq_ultra_ps_e_0/saxihp0_fpd_aclk] \
    [get_bd_pins coral_axi_wrapper_0/aclk] \
    [get_bd_pins axi_smc_ctrl/aclk] \
    [get_bd_pins axi_smc_mem/aclk] \
    [get_bd_pins rst_ps_pl/slowest_sync_clk]

# --- 5-3. 리셋 ---------------------------------------------------------------
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0] \
               [get_bd_pins rst_ps_pl/ext_reset_in]
connect_bd_net [get_bd_pins rst_ps_pl/peripheral_aresetn] \
    [get_bd_pins coral_axi_wrapper_0/aresetn] \
    [get_bd_pins axi_smc_ctrl/aresetn] \
    [get_bd_pins axi_smc_mem/aresetn]

# --- 5-4. 상수 연결 ----------------------------------------------------------
connect_bd_net [get_bd_pins const_boot_addr/dout] \
               [get_bd_pins coral_axi_wrapper_0/boot_addr]
connect_bd_net [get_bd_pins const_irq/dout] \
               [get_bd_pins coral_axi_wrapper_0/irq]

puts "\[OK\] AXI / 클럭 / 리셋 / 상수 연결 완료"

# ---------------------------------------------------------------------------
# 6. 주소 할당
#
#  LOG.md 2026-07-23 (292행): coral s_axi(reg0) = 0x5_0000_0000, range 4G
#    ITCM 0x5_0000_0000 / DTCM 0x5_0001_0000 / CSR 0x5_0003_0000
#  이 주소는 03_sw/main_*.c 의 #define CB 0x0000000500000000ULL 과 일치해야 한다.
#  바뀌면 소프트웨어가 전부 안 돈다.
# ---------------------------------------------------------------------------
assign_bd_address

set coral_segs [get_bd_addr_segs -quiet -of_objects [get_bd_cells coral_axi_wrapper_0]]
if {[llength $coral_segs] > 0} {
    foreach seg $coral_segs {
        catch {
            assign_bd_address -force \
                -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] \
                $seg -offset 0x0000000500000000 -range 4G
        }
    }
    puts "\[OK\] coral s_axi 주소 = 0x5_0000_0000 / range 4G"
} else {
    puts "\[경고\] coral 주소 세그먼트를 못 찾았습니다."
    puts "        Address Editor 를 열어 0x5_0000_0000 인지 직접 확인하세요."
    puts "        (소프트웨어의 CB 매크로와 반드시 일치해야 함)"
}

# ---------------------------------------------------------------------------
# 7. 검증 + 저장 + 래퍼 생성
# ---------------------------------------------------------------------------
validate_bd_design
regenerate_bd_layout
save_bd_design

set bd_file [get_files ${BD_NAME}.bd]
make_wrapper -files $bd_file -top -force

# make_wrapper 출력 경로는 Vivado 버전마다 다르므로 탐색해서 추가
set wrapper_v [glob -nocomplain "${PROJ_DIR}/${PROJ_NAME}.gen/sources_1/bd/${BD_NAME}/hdl/${BD_NAME}_wrapper.v"]
if {$wrapper_v eq ""} {
    set wrapper_v [glob -nocomplain "${PROJ_DIR}/${PROJ_NAME}.srcs/sources_1/bd/${BD_NAME}/hdl/${BD_NAME}_wrapper.v"]
}
if {$wrapper_v ne ""} {
    add_files -norecurse $wrapper_v
}

# ---------------------------------------------------------------------------
# 8. Top 설정
#
#  ★★ 가장 큰 함정 (LOG.md 2026-07-21)
#     Top을 coral_axi_wrapper(내부 모듈)로 두면 AXI 신호가 전부 물리 핀으로
#     노출되어 "870 I/O ports > 707 available" IO Placement 에러가 난다.
#     반드시 Block Design 전체 래퍼인 coralnpu_wrapper 를 Top으로 지정할 것.
# ---------------------------------------------------------------------------
set_property top ${BD_NAME}_wrapper [get_filesets sources_1]
update_compile_order -fileset sources_1
puts "\[OK\] Top = ${BD_NAME}_wrapper  (coral_axi_wrapper로 두면 IO 초과 에러)"

# ---------------------------------------------------------------------------
# 9. 합성 -> 구현 -> 비트스트림
# ---------------------------------------------------------------------------
puts "\n>>> 합성 시작 (수십 분 소요)"
launch_runs synth_1 -jobs $JOBS
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    error "합성 실패. Reports -> Synthesis 로그를 확인하세요."
}
puts "\[OK\] 합성 완료"

puts "\n>>> 구현 + 비트스트림 시작"
launch_runs impl_1 -to_step write_bitstream -jobs $JOBS
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    error "구현/비트스트림 실패. Reports -> Implementation 로그를 확인하세요."
}
puts "\[OK\] 비트스트림 생성 완료"

# ---------------------------------------------------------------------------
# 10. 타이밍 확인 (50MHz면 WNS 양수여야 정상)
# ---------------------------------------------------------------------------
open_run impl_1
set wns [get_property SLACK [get_timing_paths -delay_type max]]
puts "\n>>> 타이밍 WNS = $wns ns   (base 50MHz 기록: +0.462)"
if {$wns < 0} {
    puts "\[경고\] 타이밍 위반 (WNS $wns)."
    puts "        BRAM 455개로 배치가 퍼져 base보다 불리하다. 대응 순서:"
    puts "        1) 이 파일 상단 PL_FREQ 를 50 -> 40 으로 낮추고 재실행"
    puts "        2) 그래도 안 되면 33MHz"
    puts "        데모 목적상 속도는 무관하므로 클럭 하향이 가장 안전한 해법이다."
}

# ---------------------------------------------------------------------------
# 11. XSA 내보내기 (비트스트림 포함)
# ---------------------------------------------------------------------------
set xsa_path "${PROJ_DIR}/${BD_NAME}_wrapper_hm.xsa"
write_hw_platform -fixed -include_bit -force $xsa_path

puts "\n=============================================================="
puts " 재건 완료"
puts "   XSA        : $xsa_path"
puts "   비트스트림 : ${PROJ_DIR}/${PROJ_NAME}.runs/impl_1/${BD_NAME}_wrapper.bit"
puts "   타이밍 WNS : $wns ns"
puts ""
puts " 다음 단계 -> 09_remote/vitis_src/BUILD_HIGHMEM.md"
puts "=============================================================="
