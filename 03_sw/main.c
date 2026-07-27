#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"

/* =====================================================================
 *  Coral NPU (CoreMiniAxi) 실행 로더 - ZCU102 베어메탈 (A53)
 *
 *  동작: A53가 작은 RISC-V 프로그램을 NPU의 ITCM에 적재하고,
 *        DTCM에 input을 넣은 뒤 NPU를 실행시킨다.
 *        NPU는 output = input * 3 을 계산해 DTCM에 쓰고 done=1 표시.
 *        A53는 done을 폴링한 뒤 output을 읽어 UART로 출력한다.
 *
 *  주소 (PS/A53 관점, Address Editor의 s_axi = 0x5_0000_0000):
 *    ITCM +0x0     DTCM +0x10000     CSR +0x30000
 *  (오프셋은 google-coral/coralnpu 레퍼런스 드라이버 기준)
 * ===================================================================== */
#define CORAL_BASE   0x0000000500000000ULL
#define CORAL_ITCM   (CORAL_BASE + 0x00000ULL)
#define CORAL_DTCM   (CORAL_BASE + 0x10000ULL)
#define CORAL_CSR    (CORAL_BASE + 0x30000ULL)

#define CSR_CTRL     (CORAL_CSR + 0x0ULL)   /* bit0=reset 제어 */
#define CSR_PC       (CORAL_CSR + 0x4ULL)   /* 시작 주소(entry) */
#define CSR_STATUS   (CORAL_CSR + 0x8ULL)   /* 상태 */

/* DTCM 안에서 input/output/done 위치 (NPU 관점 0x10000/4/8 = A53 관점 아래) */
#define ADDR_INPUT   (CORAL_DTCM + 0x0ULL)
#define ADDR_OUTPUT  (CORAL_DTCM + 0x4ULL)
#define ADDR_DONE    (CORAL_DTCM + 0x8ULL)

/* NPU에서 돌릴 프로그램 (RISC-V rv32, 검증 완료)
 *   lui t0,0x10 ; lw t1,0(t0) ; slli t2,t1,1 ; add t2,t2,t1
 *   sw t2,4(t0) ; li t3,1 ; sw t3,8(t0) ; loop: j loop
 *   => output = input*3, done=1 후 무한루프 */
static const u32 npu_prog[] = {
    0x000102B7u, /* lui  t0, 0x10      t0 = 0x10000 (DTCM base) */
    0x0002A303u, /* lw   t1, 0(t0)     t1 = input               */
    0x00131393u, /* slli t2, t1, 1     t2 = input*2             */
    0x006383B3u, /* add  t2, t2, t1    t2 = input*3             */
    0x0072A223u, /* sw   t2, 4(t0)     output = t2              */
    0x00100E13u, /* li   t3, 1                                  */
    0x01C2A423u, /* sw   t3, 8(t0)     done = 1                 */
    0x0000006Fu, /* loop: j loop                                */
};
#define NPU_PROG_WORDS (sizeof(npu_prog)/sizeof(npu_prog[0]))

/* NPU에 프로그램을 한 번만 적재 */
static void npu_load_program(void)
{
    Xil_Out32(CSR_CTRL, 0x1);              /* 리셋 걸어 안전하게 적재 */
    for (u32 i = 0; i < NPU_PROG_WORDS; i++)
        Xil_Out32(CORAL_ITCM + i * 4, npu_prog[i]);
    Xil_Out32(CSR_PC, 0x0);                /* 시작 주소 = ITCM 0 */
}

/* input 하나를 넣고 NPU를 실행시켜 결과를 받음. 성공 시 1 반환 */
static int npu_run(u32 input, u32 *out)
{
    Xil_Out32(CSR_CTRL, 0x1);              /* 리셋 유지 */
    Xil_Out32(ADDR_INPUT, input);          /* input 주입 */
    Xil_Out32(ADDR_DONE, 0);               /* done 플래그 초기화 */
    Xil_Out32(CSR_PC, 0x0);                /* 시작 주소 재설정 */

    /* 실행: (레퍼런스 순서) ClockGate(false)=0x1 -> Reset(false)=0x0 */
    Xil_Out32(CSR_CTRL, 0x1);
    Xil_Out32(CSR_CTRL, 0x0);

    /* done 플래그 폴링 (타임아웃 포함) */
    for (volatile int t = 0; t < 2000000; t++) {
        if (Xil_In32(ADDR_DONE) == 1) {
            *out = Xil_In32(ADDR_OUTPUT);
            return 1;
        }
    }
    return 0; /* 타임아웃 */
}

int main(void)
{
    xil_printf("\r\n=== Coral NPU run: output = input * 3 ===\r\n");

    npu_load_program();

    u32 tests[] = { 7, 10, 100, 1234 };
    for (int i = 0; i < 4; i++) {
        u32 in = tests[i], out = 0;
        if (npu_run(in, &out)) {
            u32 status = Xil_In32(CSR_STATUS);
            xil_printf("  input=%-5u -> output=%-6u (expect %u) %s  [status=0x%08x]\r\n",
                       in, out, in * 3,
                       (out == in * 3) ? "OK" : "MISMATCH", status);
        } else {
            xil_printf("  input=%-5u -> TIMEOUT (NPU 응답 없음)\r\n", in);
        }
    }

    xil_printf("=== done ===\r\n");
    return 0;
}