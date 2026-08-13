#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"

/* =====================================================================
 *  Coral NPU - 완전연결층(FC layer) 추론 데모  Y = ReLU(W*X + b)
 *  ZCU102 베어메탈 (A53)
 *
 *  Coral NPU(RISC-V 코어)가 4x4 행렬 W 와 입력벡터 X(4) 를 곱하고
 *  편향 b(4) 를 더한 뒤 ReLU 활성화까지 수행해 출력 Y(4) 를 만든다.
 *  = 신경망 한 층(뉴런 4개)의 추론.
 *
 *  주소 (A53 관점, s_axi = 0x5_0000_0000):
 *    ITCM +0x0     DTCM +0x10000     CSR +0x30000
 *  DTCM 내부 배치: W +0x00(16) / X +0x40(4) / b +0x50(4) / Y +0x60(4) / done +0x70
 * ===================================================================== */
#define CORAL_BASE   0x0000000500000000ULL
#define CORAL_ITCM   (CORAL_BASE + 0x00000ULL)
#define CORAL_DTCM   (CORAL_BASE + 0x10000ULL)
#define CORAL_CSR    (CORAL_BASE + 0x30000ULL)
#define CSR_CTRL     (CORAL_CSR + 0x0ULL)
#define CSR_PC       (CORAL_CSR + 0x4ULL)

#define OFF_W    0x00ULL
#define OFF_X    0x40ULL
#define OFF_B    0x50ULL
#define OFF_Y    0x60ULL
#define OFF_DONE 0x70ULL

/* NPU에서 돌릴 프로그램 (RISC-V rv32i, mul 명령 미사용 - shift+add 곱셈)
 * 어셈블 + 에뮬레이터 3케이스 검증 완료
 * Y[i] = ReLU( sum_j W[i][j]*X[j] + b[i] ), i,j = 0..3 */
static const u32 npu_prog[] = {
    0x000102B7u, 0x00000513u, 0x00400313u, 0x06650A63u,
    0x00000613u, 0x00000593u, 0x00451393u, 0x00728E33u,
    0x00400313u, 0x02658663u, 0x00259F13u, 0x01EE0FB3u,
    0x000FA403u, 0x04028493u, 0x01E484B3u, 0x0004A483u,
    0x050000EFu, 0x01260633u, 0x00158593u, 0xFD5FF06Fu,
    0x05028A13u, 0x00251393u, 0x007A0A33u, 0x000A2A83u,
    0x01560633u, 0x00065463u, 0x00000613u, 0x06028B13u,
    0x007B0B33u, 0x00CB2023u, 0x00150513u, 0xF8DFF06Fu,
    0x00100B93u, 0x07028C13u, 0x017C2023u, 0x0000006Fu,
    0x00000913u, 0x00048E63u, 0x0014F993u, 0x00098463u,
    0x00890933u, 0x00141413u, 0x0014D493u, 0xFE9FF06Fu,
    0x00008067u,
};
#define NPU_PROG_WORDS (sizeof(npu_prog)/sizeof(npu_prog[0]))

static void npu_load_program(void)
{
    Xil_Out32(CSR_CTRL, 0x1);
    for (u32 i = 0; i < NPU_PROG_WORDS; i++)
        Xil_Out32(CORAL_ITCM + i * 4, npu_prog[i]);
    Xil_Out32(CSR_PC, 0x0);
}

/* W(4x4 row-major), X(4), b(4) 를 넣고 NPU 실행 후 Y(4) 를 읽음. 성공 시 1 */
static int npu_run_fc(const int *W, const int *X, const int *b, int *Y)
{
    Xil_Out32(CSR_CTRL, 0x1);                       /* 리셋 유지 */
    for (int i = 0; i < 16; i++) Xil_Out32(CORAL_DTCM + OFF_W + i*4, (u32)W[i]);
    for (int i = 0; i < 4;  i++) Xil_Out32(CORAL_DTCM + OFF_X + i*4, (u32)X[i]);
    for (int i = 0; i < 4;  i++) Xil_Out32(CORAL_DTCM + OFF_B + i*4, (u32)b[i]);
    Xil_Out32(CORAL_DTCM + OFF_DONE, 0);
    Xil_Out32(CSR_PC, 0x0);

    Xil_Out32(CSR_CTRL, 0x1);                       /* 실행: 0x1 -> 0x0 */
    Xil_Out32(CSR_CTRL, 0x0);

    for (volatile int t = 0; t < 5000000; t++) {
        if (Xil_In32(CORAL_DTCM + OFF_DONE) == 1) {
            for (int i = 0; i < 4; i++)
                Y[i] = (int)Xil_In32(CORAL_DTCM + OFF_Y + i*4);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    xil_printf("\r\n=== Coral NPU: Fully-Connected Layer  Y = ReLU(W*X + b) ===\r\n");

    /* 테스트 가중치/입력/편향 (정수) */
    int W[16] = {  1, -2,  0,  3,
                   2,  1, -1,  0,
                   0,  0,  2,  1,
                  -1,  4,  1,  2 };
    int X[4]  = {  3,  1,  2,  5 };
    int b[4]  = {  0, -50, 1, 100 };
    int Y[4]  = { 0 };

    npu_load_program();

    if (!npu_run_fc(W, X, b, Y)) {
        xil_printf("TIMEOUT: NPU 응답 없음\r\n");
        return 1;
    }

    /* 입력 출력 */
    xil_printf("\r\nW (4x4):\r\n");
    for (int i = 0; i < 4; i++)
        xil_printf("  [ %4d %4d %4d %4d ]\r\n", W[i*4+0], W[i*4+1], W[i*4+2], W[i*4+3]);
    xil_printf("X = [ %d %d %d %d ]\r\n", X[0], X[1], X[2], X[3]);
    xil_printf("b = [ %d %d %d %d ]\r\n", b[0], b[1], b[2], b[3]);

    /* NPU 결과 vs CPU 기대값 */
    xil_printf("\r\nNPU  Y = [ %d %d %d %d ]\r\n", Y[0], Y[1], Y[2], Y[3]);

    int ok = 1;
    xil_printf("check:\r\n");
    for (int i = 0; i < 4; i++) {
        int acc = b[i];
        for (int j = 0; j < 4; j++) acc += W[i*4+j] * X[j];
        if (acc < 0) acc = 0;                       /* ReLU */
        xil_printf("  Y[%d]=%-5d expect %-5d %s\r\n",
                   i, Y[i], acc, (Y[i]==acc) ? "OK" : "MISMATCH");
        if (Y[i] != acc) ok = 0;
    }

    xil_printf(ok ? "\r\nRESULT: PASS - NPU가 신경망 한 층을 계산했습니다!\r\n"
                  : "\r\nRESULT: FAIL\r\n");
    xil_printf("=== done ===\r\n");
    return 0;
}