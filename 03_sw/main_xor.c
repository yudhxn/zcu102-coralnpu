#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"

/* 2층 신경망 (2->2->1, ReLU) 이 XOR을 계산하는 데모. mul 사용.
 * DTCM 배치(NPU 관점): X 0x00 / W1 0x10 / b1 0x30 / hidden 0x40 / W2 0x50 / b2 0x60 / out 0x70 / done 0x80 */
#define CB   0x0000000500000000ULL
#define ITCM (CB + 0x00000ULL)
#define DTCM (CB + 0x10000ULL)
#define CSR  (CB + 0x30000ULL)
#define CTRL (CSR + 0x0ULL)
#define PC   (CSR + 0x4ULL)

static const u32 prog[] = {
    0x000102B7u,0x0002A503u,0x0042A583u,0x0102A303u,0x0142A383u,0x02A30333u,0x02B383B3u,0x00730E33u,
    0x0302AE83u,0x01DE0E33u,0x000E5463u,0x00000E13u,0x05C2A023u,0x0182A303u,0x01C2A383u,0x02A30333u,
    0x02B383B3u,0x00730F33u,0x0342AE83u,0x01DF0F33u,0x000F5463u,0x00000F13u,0x05E2A223u,0x0502A303u,
    0x0542A383u,0x03C30333u,0x03E383B3u,0x00730FB3u,0x0602AE83u,0x01DF8FB3u,0x07F2A823u,0x00100313u,
    0x0862A023u,0x0000006Fu,
};
#define NW (sizeof(prog)/sizeof(prog[0]))

int main(void)
{
    xil_printf("\r\n=== NPU 2-layer NN solves XOR (mul) ===\r\n");
    Xil_Out32(CTRL, 0x1);
    for (u32 i=0;i<NW;i++) Xil_Out32(ITCM+i*4, prog[i]);
    /* 가중치: W1=[[1,1],[1,1]] b1=[0,-1] W2=[1,-2] b2=0 */
    int W1[4]={1,1,1,1}, b1[2]={0,-1}, W2[2]={1,-2}, b2=0;
    for(int i=0;i<4;i++) Xil_Out32(DTCM+0x10+i*4,(u32)W1[i]);
    Xil_Out32(DTCM+0x30,(u32)b1[0]); Xil_Out32(DTCM+0x34,(u32)b1[1]);
    for(int i=0;i<2;i++) Xil_Out32(DTCM+0x50+i*4,(u32)W2[i]);
    Xil_Out32(DTCM+0x60,(u32)b2);

    int inputs[4][2]={{0,0},{0,1},{1,0},{1,1}};
    int ok=1;
    for(int t=0;t<4;t++){
        int x0=inputs[t][0], x1=inputs[t][1];
        Xil_Out32(DTCM+0x00,(u32)x0); Xil_Out32(DTCM+0x04,(u32)x1);
        Xil_Out32(DTCM+0x80,0);
        Xil_Out32(PC,0x0); Xil_Out32(CTRL,0x1); Xil_Out32(CTRL,0x0);
        u32 done=0; for(volatile int k=0;k<2000000;k++){done=Xil_In32(DTCM+0x80); if(done)break;}
        if(!done){ xil_printf("  (%d,%d) TIMEOUT\r\n",x0,x1); ok=0; continue; }
        int out=(int)Xil_In32(DTCM+0x70);
        int exp=x0^x1;
        xil_printf("  XOR(%d,%d) -> NPU=%d  expect=%d  %s\r\n",x0,x1,out,exp,(out==exp)?"OK":"X");
        if(out!=exp) ok=0;
    }
    xil_printf(ok? "\r\nRESULT: PASS - NPU가 XOR 신경망을 계산!\r\n":"\r\nRESULT: FAIL\r\n");
    xil_printf("=== done ===\r\n");
    return 0;
}
