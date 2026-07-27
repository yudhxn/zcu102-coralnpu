#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"

/* =====================================================================
 *  3층 MLP (4 -> 8 -> 8 -> 3, ReLU) 를 Coral NPU에서 추론
 *  ★ 이 NPU 프로그램은 C로 작성해 riscv64-unknown-elf-gcc 로
 *    rv32im 빌드한 결과(.text)를 그대로 적재한 것 (손 어셈블 아님)
 *
 *  DTCM 배치: IN 0x00 / W1 0x20 / B1 0xA0 / W2 0xC0 / B2 0x1C0
 *             W3 0x1E0 / B3 0x240 / OUT 0x250 / ARGMAX 0x25C / DONE 0x260
 *             (내부 임시 h1,h2는 0x14000 이후, 링커로 분리)
 * ===================================================================== */
#define CB   0x0000000500000000ULL
#define ITCM (CB + 0x00000ULL)
#define DTCM (CB + 0x10000ULL)
#define CSR  (CB + 0x30000ULL)
#define CTRL (CSR + 0x0ULL)
#define PCR  (CSR + 0x4ULL)

#define O_IN 0x00ULL
#define O_W1 0x20ULL
#define O_B1 0xA0ULL
#define O_W2 0xC0ULL
#define O_B2 0x1C0ULL
#define O_W3 0x1E0ULL
#define O_B3 0x240ULL
#define O_OUT 0x250ULL
#define O_ARG 0x25CULL
#define O_DONE 0x260ULL

static const u32 npu_prog[] = {
    0x00014337u, 0x00010837u, 0x00030E13u, 0x00030513u, 0x0A080893u, 0x00030313u,
    0x00000E93u, 0x01080813u, 0x00800F13u, 0x0008A683u, 0xF8088613u, 0x000107B7u,
    0x00062703u, 0x0007A583u, 0x00478793u, 0x02060613u, 0x02B70733u, 0x00E686B3u,
    0xFF0794E3u, 0xFFF6C793u, 0x41F7D793u, 0x00F6F6B3u, 0x00D32023u, 0x001E8E93u,
    0x00488893u, 0x00430313u, 0xFBEE9EE3u, 0x02050513u, 0x00010837u, 0x00050313u,
    0x1C080813u, 0x00000893u, 0x00800E93u, 0x00082603u, 0xF0080693u, 0x000E0793u,
    0x0006A703u, 0x0007A583u, 0x00478793u, 0x02068693u, 0x02B70733u, 0x00E60633u,
    0xFEF514E3u, 0xFFF64793u, 0x41F7D793u, 0x00F67633u, 0x00C32023u, 0x00188893u,
    0x00480813u, 0x00430313u, 0xFBD89EE3u, 0x000108B7u, 0x80000E37u, 0x00014837u,
    0x24088893u, 0x00000313u, 0x001E0E13u, 0x00000F13u, 0x04080813u, 0x00300E93u,
    0x0008A603u, 0xFA088693u, 0x00050793u, 0x0006A703u, 0x0007A583u, 0x00478793u,
    0x00C68693u, 0x02B70733u, 0x00E60633u, 0xFEF814E3u, 0x00C8A823u, 0x00CE5663u,
    0x00060E13u, 0x00030F13u, 0x00130313u, 0x00488893u, 0xFDD310E3u, 0x000107B7u,
    0x25E7AE23u, 0x00100713u, 0x26E7A023u, 0x0000006Fu,
};

#define NW (sizeof(npu_prog)/sizeof(npu_prog[0]))

static const int W1[32] = {
    -1, -2, 0, 2, -3, -3, 3, 1, -3, -1, 1, -3,
    1, -2, -3, -3, 0, 0, -3, -2, -3, 1, 0, -3,
    3, 1, -3, -2, 2, 2, 1, -3,
};
static const int B1[8] = {
    4, 4, 1, -5, -2, -5, 3, -3,
};
static const int W2[64] = {
    -1, 0, -2, 1, -3, 1, -1, 1, 3, 2, -2, -3,
    1, 1, 2, -2, -1, -3, 1, 2, -3, 1, -3, 1,
    -2, 0, 2, 1, 0, 3, -1, 0, 1, 0, -1, -1,
    -2, 3, -2, 2, 3, -2, -3, 1, -1, 1, 0, -1,
    2, 0, -1, 1, -3, -3, 1, 0, -2, 3, -1, -2,
    0, 0, -3, 2,
};
static const int B2[8] = {
    -4, 3, 4, 0, 0, 0, 4, 2,
};
static const int W3[24] = {
    1, 3, 0, -3, 3, -3, -1, 0, 2, 2, -3, -3,
    2, 2, -1, 2, 1, 2, 3, 0, -1, 2, 0, 2,
};
static const int B3[3] = {
    0, -5, 2,
};

static void put(u64 off, const int *a, int n)
{
    for (int i = 0; i < n; i++) Xil_Out32(DTCM + off + i*4, (u32)a[i]);
}

int main(void)
{
    xil_printf("\r\n=== NPU 3-layer MLP (4-8-8-3), built from C via riscv gcc ===\r\n");

    Xil_Out32(CTRL, 0x1);
    for (u32 i = 0; i < NW; i++) Xil_Out32(ITCM + i*4, npu_prog[i]);
    put(O_W1, W1, 32); put(O_B1, B1, 8);
    put(O_W2, W2, 64); put(O_B2, B2, 8);
    put(O_W3, W3, 24); put(O_B3, B3, 3);

    int tests[4][4] = { {1,2,3,4}, {5,0,-2,1}, {0,0,0,0}, {-3,7,2,-1} };
    int expect[4][3] = { {5,47,-33}, {74,-53,-58}, {9,46,-30}, {83,29,82} };
    int ok = 1;

    for (int t = 0; t < 4; t++) {
        put(O_IN, tests[t], 4);
        Xil_Out32(DTCM + O_DONE, 0);
        Xil_Out32(PCR, 0x0);
        Xil_Out32(CTRL, 0x1);
        Xil_Out32(CTRL, 0x0);

        u32 done = 0;
        for (volatile int k = 0; k < 8000000; k++) { done = Xil_In32(DTCM + O_DONE); if (done) break; }
        if (!done) { xil_printf("  test%d TIMEOUT\r\n", t); ok = 0; continue; }

        int o0=(int)Xil_In32(DTCM+O_OUT), o1=(int)Xil_In32(DTCM+O_OUT+4), o2=(int)Xil_In32(DTCM+O_OUT+8);
        int am=(int)Xil_In32(DTCM+O_ARG);
        int good = (o0==expect[t][0] && o1==expect[t][1] && o2==expect[t][2]);
        xil_printf("  in=[%d %d %d %d] -> out=[%d %d %d] argmax=%d  %s\r\n",
                   tests[t][0],tests[t][1],tests[t][2],tests[t][3], o0,o1,o2, am,
                   good ? "OK" : "MISMATCH");
        if (!good) ok = 0;
    }

    xil_printf(ok ? "\r\nRESULT: PASS - C로 짠 3층 신경망이 NPU에서 추론!\r\n"
                  : "\r\nRESULT: FAIL\r\n");
    xil_printf("=== done ===\r\n");
    return 0;
}
