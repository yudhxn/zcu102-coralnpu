// ============================================================================
// coral_axi_wrapper_vme.sv   (2026-08-12 생성)
//
// 완전한 Coral NPU(VmeCoreMiniAxi = 스칼라 + 벡터/SIMD + 행렬)를 Xilinx 표준
// AXI 이름으로 감싸는 껍데기. 기존 coral_axi_wrapper.sv 와 포트가 100% 같다.
//
// 기존본과의 유일한 차이
//   - 인스턴스가 CoreMiniAxi -> VmeCoreMiniAxi
//   - io_debug_* 108개 연결 삭제 (완전체 코어는 디버그 포트를 노출하지 않음)
//
// ★ 주의: 모듈 이름이 coral_axi_wrapper 와 달라야 한다. 같은 이름이면
//         두 파일을 함께 프로젝트에 넣을 때 module redefinition 에러가 난다.
//
// ★ 합성 시 필수 define (RTL 직접 확인, 2026-08-12)
//     SYNTHESIS / FPGA_XILINX / ZVE32F_ON / ZVT_ON
// ============================================================================

module coral_axi_wrapper_vme (
    input  wire         aclk,
    input  wire         aresetn,

    // ---------------- S_AXI : PS -> Coral (제어) ----------------
    // Write address
    input  wire [5:0]   s_axi_awid,
    input  wire [31:0]  s_axi_awaddr,
    input  wire [7:0]   s_axi_awlen,
    input  wire [2:0]   s_axi_awsize,
    input  wire [1:0]   s_axi_awburst,
    input  wire         s_axi_awlock,
    input  wire [3:0]   s_axi_awcache,
    input  wire [2:0]   s_axi_awprot,
    input  wire [3:0]   s_axi_awqos,
    input  wire [3:0]   s_axi_awregion,
    input  wire         s_axi_awvalid,
    output wire         s_axi_awready,
    // Write data
    input  wire [127:0] s_axi_wdata,
    input  wire [15:0]  s_axi_wstrb,
    input  wire         s_axi_wlast,
    input  wire         s_axi_wvalid,
    output wire         s_axi_wready,
    // Write response
    output wire [5:0]   s_axi_bid,
    output wire [1:0]   s_axi_bresp,
    output wire         s_axi_bvalid,
    input  wire         s_axi_bready,
    // Read address
    input  wire [5:0]   s_axi_arid,
    input  wire [31:0]  s_axi_araddr,
    input  wire [7:0]   s_axi_arlen,
    input  wire [2:0]   s_axi_arsize,
    input  wire [1:0]   s_axi_arburst,
    input  wire         s_axi_arlock,
    input  wire [3:0]   s_axi_arcache,
    input  wire [2:0]   s_axi_arprot,
    input  wire [3:0]   s_axi_arqos,
    input  wire [3:0]   s_axi_arregion,
    input  wire         s_axi_arvalid,
    output wire         s_axi_arready,
    // Read data
    output wire [5:0]   s_axi_rid,
    output wire [127:0] s_axi_rdata,
    output wire [1:0]   s_axi_rresp,
    output wire         s_axi_rlast,
    output wire         s_axi_rvalid,
    input  wire         s_axi_rready,

    // ---------------- M_AXI : Coral -> 메모리 ----------------
    // Write address
    output wire [5:0]   m_axi_awid,
    output wire [31:0]  m_axi_awaddr,
    output wire [7:0]   m_axi_awlen,
    output wire [2:0]   m_axi_awsize,
    output wire [1:0]   m_axi_awburst,
    output wire         m_axi_awlock,
    output wire [3:0]   m_axi_awcache,
    output wire [2:0]   m_axi_awprot,
    output wire [3:0]   m_axi_awqos,
    output wire [3:0]   m_axi_awregion,
    output wire         m_axi_awvalid,
    input  wire         m_axi_awready,
    // Write data
    output wire [127:0] m_axi_wdata,
    output wire [15:0]  m_axi_wstrb,
    output wire         m_axi_wlast,
    output wire         m_axi_wvalid,
    input  wire         m_axi_wready,
    // Write response
    input  wire [5:0]   m_axi_bid,
    input  wire [1:0]   m_axi_bresp,
    input  wire         m_axi_bvalid,
    output wire         m_axi_bready,
    // Read address
    output wire [5:0]   m_axi_arid,
    output wire [31:0]  m_axi_araddr,
    output wire [7:0]   m_axi_arlen,
    output wire [2:0]   m_axi_arsize,
    output wire [1:0]   m_axi_arburst,
    output wire         m_axi_arlock,
    output wire [3:0]   m_axi_arcache,
    output wire [2:0]   m_axi_arprot,
    output wire [3:0]   m_axi_arqos,
    output wire [3:0]   m_axi_arregion,
    output wire         m_axi_arvalid,
    input  wire         m_axi_arready,
    // Read data
    input  wire [5:0]   m_axi_rid,
    input  wire [127:0] m_axi_rdata,
    input  wire [1:0]   m_axi_rresp,
    input  wire         m_axi_rlast,
    input  wire         m_axi_rvalid,
    output wire         m_axi_rready,

    // ---------------- 간단 제어 ----------------
    input  wire [31:0]  boot_addr,   // Coral 부팅 시작 주소
    input  wire         irq,         // 외부 인터럽트
    output wire         halted,      // 코어 정지 상태
    output wire         fault        // 코어 fault 상태
);

  // ==========================================================================
  // CoreMiniAxi 인스턴스: 표준 AXI 이름 <-> Chisel 이름 매핑
  // ==========================================================================
  VmeCoreMiniAxi u_core (
      .io_aclk    (aclk),
      .io_aresetn (aresetn),

      // ---- S_AXI: slave write address ----
      .io_axi_slave_write_addr_ready       (s_axi_awready),
      .io_axi_slave_write_addr_valid       (s_axi_awvalid),
      .io_axi_slave_write_addr_bits_addr   (s_axi_awaddr),
      .io_axi_slave_write_addr_bits_prot   (s_axi_awprot),
      .io_axi_slave_write_addr_bits_id     (s_axi_awid),
      .io_axi_slave_write_addr_bits_len    (s_axi_awlen),
      .io_axi_slave_write_addr_bits_size   (s_axi_awsize),
      .io_axi_slave_write_addr_bits_burst  (s_axi_awburst),
      .io_axi_slave_write_addr_bits_lock   (s_axi_awlock),
      .io_axi_slave_write_addr_bits_cache  (s_axi_awcache),
      .io_axi_slave_write_addr_bits_qos    (s_axi_awqos),
      .io_axi_slave_write_addr_bits_region (s_axi_awregion),
      // slave write data
      .io_axi_slave_write_data_ready       (s_axi_wready),
      .io_axi_slave_write_data_valid       (s_axi_wvalid),
      .io_axi_slave_write_data_bits_data   (s_axi_wdata),
      .io_axi_slave_write_data_bits_last   (s_axi_wlast),
      .io_axi_slave_write_data_bits_strb   (s_axi_wstrb),
      // slave write resp
      .io_axi_slave_write_resp_ready       (s_axi_bready),
      .io_axi_slave_write_resp_valid       (s_axi_bvalid),
      .io_axi_slave_write_resp_bits_id     (s_axi_bid),
      .io_axi_slave_write_resp_bits_resp   (s_axi_bresp),
      // slave read address
      .io_axi_slave_read_addr_ready        (s_axi_arready),
      .io_axi_slave_read_addr_valid        (s_axi_arvalid),
      .io_axi_slave_read_addr_bits_addr    (s_axi_araddr),
      .io_axi_slave_read_addr_bits_prot    (s_axi_arprot),
      .io_axi_slave_read_addr_bits_id      (s_axi_arid),
      .io_axi_slave_read_addr_bits_len     (s_axi_arlen),
      .io_axi_slave_read_addr_bits_size    (s_axi_arsize),
      .io_axi_slave_read_addr_bits_burst   (s_axi_arburst),
      .io_axi_slave_read_addr_bits_lock    (s_axi_arlock),
      .io_axi_slave_read_addr_bits_cache   (s_axi_arcache),
      .io_axi_slave_read_addr_bits_qos     (s_axi_arqos),
      .io_axi_slave_read_addr_bits_region  (s_axi_arregion),
      // slave read data
      .io_axi_slave_read_data_ready        (s_axi_rready),
      .io_axi_slave_read_data_valid        (s_axi_rvalid),
      .io_axi_slave_read_data_bits_data    (s_axi_rdata),
      .io_axi_slave_read_data_bits_id      (s_axi_rid),
      .io_axi_slave_read_data_bits_resp    (s_axi_rresp),
      .io_axi_slave_read_data_bits_last    (s_axi_rlast),

      // ---- M_AXI: master write address ----
      .io_axi_master_write_addr_ready      (m_axi_awready),
      .io_axi_master_write_addr_valid      (m_axi_awvalid),
      .io_axi_master_write_addr_bits_addr  (m_axi_awaddr),
      .io_axi_master_write_addr_bits_prot  (m_axi_awprot),
      .io_axi_master_write_addr_bits_id    (m_axi_awid),
      .io_axi_master_write_addr_bits_len   (m_axi_awlen),
      .io_axi_master_write_addr_bits_size  (m_axi_awsize),
      .io_axi_master_write_addr_bits_burst (m_axi_awburst),
      .io_axi_master_write_addr_bits_lock  (m_axi_awlock),
      .io_axi_master_write_addr_bits_cache (m_axi_awcache),
      .io_axi_master_write_addr_bits_qos   (m_axi_awqos),
      .io_axi_master_write_addr_bits_region(m_axi_awregion),
      // master write data
      .io_axi_master_write_data_ready      (m_axi_wready),
      .io_axi_master_write_data_valid      (m_axi_wvalid),
      .io_axi_master_write_data_bits_data  (m_axi_wdata),
      .io_axi_master_write_data_bits_last  (m_axi_wlast),
      .io_axi_master_write_data_bits_strb  (m_axi_wstrb),
      // master write resp
      .io_axi_master_write_resp_ready      (m_axi_bready),
      .io_axi_master_write_resp_valid      (m_axi_bvalid),
      .io_axi_master_write_resp_bits_id    (m_axi_bid),
      .io_axi_master_write_resp_bits_resp  (m_axi_bresp),
      // master read address
      .io_axi_master_read_addr_ready       (m_axi_arready),
      .io_axi_master_read_addr_valid       (m_axi_arvalid),
      .io_axi_master_read_addr_bits_addr   (m_axi_araddr),
      .io_axi_master_read_addr_bits_prot   (m_axi_arprot),
      .io_axi_master_read_addr_bits_id     (m_axi_arid),
      .io_axi_master_read_addr_bits_len    (m_axi_arlen),
      .io_axi_master_read_addr_bits_size   (m_axi_arsize),
      .io_axi_master_read_addr_bits_burst  (m_axi_arburst),
      .io_axi_master_read_addr_bits_lock   (m_axi_arlock),
      .io_axi_master_read_addr_bits_cache  (m_axi_arcache),
      .io_axi_master_read_addr_bits_qos    (m_axi_arqos),
      .io_axi_master_read_addr_bits_region (m_axi_arregion),
      // master read data
      .io_axi_master_read_data_ready       (m_axi_rready),
      .io_axi_master_read_data_valid       (m_axi_rvalid),
      .io_axi_master_read_data_bits_data   (m_axi_rdata),
      .io_axi_master_read_data_bits_id     (m_axi_rid),
      .io_axi_master_read_data_bits_resp   (m_axi_rresp),
      .io_axi_master_read_data_bits_last   (m_axi_rlast),

      // ---- 상태 / 제어 ----
      .io_halted       (halted),
      .io_fault        (fault),
      .io_wfi          (/* unused */),
      .io_irq          (irq),
      .io_boot_addr    (boot_addr),
      .io_timer_irq    (1'b0),
      .io_software_irq (1'b0),


      // ---- Debug Module (JTAG) 포트: 사용 안 함 ----
      .io_dm_req_ready        (/* open */),
      .io_dm_req_valid        (1'b0),
      .io_dm_req_bits_address (32'b0),
      .io_dm_req_bits_data    (32'b0),
      .io_dm_req_bits_op      (2'b0),
      .io_dm_rsp_ready        (1'b0),
      .io_dm_rsp_valid        (/* open */),
      .io_dm_rsp_bits_data    (/* open */),
      .io_dm_rsp_bits_op      (/* open */),
      .io_te                  (1'b0)
  );

endmodule
