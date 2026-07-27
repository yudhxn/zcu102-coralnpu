#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"

/* =====================================================================
 *  M-확장(mul) 진단 - 왜 mul이 보드에서 멈추는지 원인 규명
 *
 *  NPU에 "트랩 핸들러를 걸고 mul을 실행"하는 프로그램을 올린다.
 *  결과(DTCM+8 done)로 세 경우를 구분:
 *    done=1 : mul 정상 동작 (result=6*7=42)         -> M-확장 있음
 *    done=2 : mul이 트랩(예외) 발생 (mcause 기록)     -> 불법명령, M-확장 없음
 *    done=0 : 아무 일 없이 코어가 멈춤 (트랩도 안 남)  -> 곱셈기 HW 행(hang)
 * ===================================================================== */
#define CORAL_BASE   0x0000000500000000ULL
#define CORAL_ITCM   (CORAL_BASE + 0x00000ULL)
#define CORAL_DTCM   (CORAL_BASE + 0x10000ULL)
#define CORAL_CSR    (CORAL_BASE + 0x30000ULL)
#define CSR_CTRL     (CORAL_CSR + 0x0ULL)
#define CSR_PC       (CORAL_CSR + 0x4ULL)

#define RES   (CORAL_DTCM + 0x0ULL)   /* mul 결과       */
#define MCSE  (CORAL_DTCM + 0x4ULL)   /* 트랩 시 mcause */
#define DONE  (CORAL_DTCM + 0x8ULL)   /* 0/1/2 상태     */

/* 진단 프로그램 (rv32, 어셈블+에뮬 검증)
 *   mtvec=handler 설정 -> mul x31,6,7 -> 성공시 done=1
 *   트랩 시 handler에서 mcause 읽어 done=2 */
static const u32 npu_prog[] = {
    0x000102B7u, 0x02C00E13u, 0x305E1073u, 0x0002A423u,
    0x00600E93u, 0x00700F13u, 0x03EE8FB3u, 0x01F2A023u,
    0x00100313u, 0x0062A423u, 0x0000006Fu, 0x34202373u,
    0x0062A223u, 0x00200393u, 0x0072A423u, 0x0000006Fu,
};
#define NW (sizeof(npu_prog)/sizeof(npu_prog[0]))

int main(void)
{
    xil_printf("\r\n=== M-extension (mul) diagnostic ===\r\n");

    Xil_Out32(CSR_CTRL, 0x1);                 /* 리셋 유지 */
    for (u32 i = 0; i < NW; i++) Xil_Out32(CORAL_ITCM + i*4, npu_prog[i]);
    Xil_Out32(DONE, 0);
    Xil_Out32(MCSE, 0);
    Xil_Out32(RES, 0);
    Xil_Out32(CSR_PC, 0x0);

    Xil_Out32(CSR_CTRL, 0x1);                 /* 실행: 0x1 -> 0x0 */
    Xil_Out32(CSR_CTRL, 0x0);

    u32 done = 0;
    for (volatile int t = 0; t < 3000000; t++) {
        done = Xil_In32(DONE);
        if (done != 0) break;
    }

    u32 res = Xil_In32(RES), mc = Xil_In32(MCSE);

    if (done == 1) {
        xil_printf("결과: done=1  result=%d (기대 42)\r\n", (int)res);
        xil_printf(">> mul 정상 동작 = M-확장 있음. (앞선 멈춤은 다른 원인일 수 있음)\r\n");
    } else if (done == 2) {
        xil_printf("결과: done=2  mcause=0x%08x\r\n", mc);
        xil_printf(">> mul이 트랩(예외) 발생. mcause=2 이면 '불법 명령' = M-확장 없음.\r\n");
    } else {
        xil_printf("결과: done=0 (타임아웃)\r\n");
        xil_printf(">> 트랩도 없이 코어가 멈춤 = 곱셈기 하드웨어 행(hang). M-확장 미구현/버그.\r\n");
    }

    xil_printf("=== done ===\r\n");
    return 0;
}
