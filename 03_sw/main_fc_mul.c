#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"

/* FC 레이어 재검증 - 이번엔 mul(하드웨어 곱셈) 버전으로 다시 실행.
 * mul 단독은 정상(result=42)임을 확인했으니, 이 버전이
 *   - 정상 동작하면  -> 앞선 멈춤은 일시적 문제였음
 *   - 또 멈추면      -> 반복문 내 mul 특정 상황의 문제 (추가 조사)  */
#define CORAL_BASE   0x0000000500000000ULL
#define CORAL_ITCM   (CORAL_BASE + 0x00000ULL)
#define CORAL_DTCM   (CORAL_BASE + 0x10000ULL)
#define CORAL_CSR    (CORAL_BASE + 0x30000ULL)
#define CSR_CTRL     (CORAL_CSR + 0x0ULL)
#define CSR_PC       (CORAL_CSR + 0x4ULL)
#define OFF_W 0x00ULL
#define OFF_X 0x40ULL
#define OFF_B 0x50ULL
#define OFF_Y 0x60ULL
#define OFF_DONE 0x70ULL

/* FC 레이어, mul 사용 버전 (어셈블+에뮬 검증). Y[i]=ReLU(sum W[i][j]*X[j]+b[i]) */
static const u32 npu_prog[] = {
    0x000102B7u, 0x00000513u, 0x00400313u, 0x06650A63u,
    0x00000613u, 0x00000593u, 0x00451393u, 0x00728E33u,
    0x00400E93u, 0x03D58663u, 0x00259F13u, 0x01EE0FB3u,
    0x000FA403u, 0x04028493u, 0x01E484B3u, 0x0004A903u,
    0x032409B3u, 0x01360633u, 0x00158593u, 0xFD5FF06Fu,
    0x05028A13u, 0x00251393u, 0x007A0A33u, 0x000A2A83u,
    0x01560633u, 0x00065463u, 0x00000613u, 0x06028B13u,
    0x007B0B33u, 0x00CB2023u, 0x00150513u, 0xF8DFF06Fu,
    0x00100B93u, 0x07028C13u, 0x017C2023u, 0x0000006Fu,
};
#define NW (sizeof(npu_prog)/sizeof(npu_prog[0]))

int main(void)
{
    xil_printf("\r\n=== FC layer (mul version) re-test ===\r\n");

    Xil_Out32(CSR_CTRL, 0x1);
    for (u32 i = 0; i < NW; i++) Xil_Out32(CORAL_ITCM + i*4, npu_prog[i]);

    int W[16] = { 1,-2,0,3, 2,1,-1,0, 0,0,2,1, -1,4,1,2 };
    int X[4]  = { 3,1,2,5 };
    int b[4]  = { 0,-50,1,100 };
    for (int i=0;i<16;i++) Xil_Out32(CORAL_DTCM+OFF_W+i*4,(u32)W[i]);
    for (int i=0;i<4;i++)  Xil_Out32(CORAL_DTCM+OFF_X+i*4,(u32)X[i]);
    for (int i=0;i<4;i++)  Xil_Out32(CORAL_DTCM+OFF_B+i*4,(u32)b[i]);
    Xil_Out32(CORAL_DTCM+OFF_DONE, 0);
    Xil_Out32(CSR_PC, 0x0);

    Xil_Out32(CSR_CTRL, 0x1);
    Xil_Out32(CSR_CTRL, 0x0);

    u32 done = 0;
    for (volatile int t=0; t<5000000; t++){ done=Xil_In32(CORAL_DTCM+OFF_DONE); if(done)break; }

    if (done == 1) {
        int Y[4]; for(int i=0;i<4;i++) Y[i]=(int)Xil_In32(CORAL_DTCM+OFF_Y+i*4);
        xil_printf("NPU Y = [ %d %d %d %d ]  (기대 16 0 10 113)\r\n", Y[0],Y[1],Y[2],Y[3]);
        int ok = (Y[0]==16&&Y[1]==0&&Y[2]==10&&Y[3]==113);
        xil_printf(ok ? ">> mul 버전 FC 정상! 앞선 멈춤은 일시적 문제였음.\r\n"
                      : ">> 동작하나 값 불일치 - 추가 확인 필요.\r\n");
    } else {
        xil_printf(">> 또 멈춤(done=0). 반복문 내 mul 특정 상황 문제로 추정 - 추가 조사.\r\n");
    }
    xil_printf("=== done ===\r\n");
    return 0;
}
