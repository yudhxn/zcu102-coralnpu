/* main_mnist28.c — 28x28 실제 MNIST를 Coral NPU에서 추론 (베어메탈, A53)
 *
 *  기존 main_mnist.c(8x8)의 확장. 2026-08-12 작성.
 *
 *  모델 : FC 784 -> 32(ReLU) -> 10, 대칭 int8
 *         float32 96.36%  ->  int8 96.41%  (테스트 10,000장, 양자화 손실 없음)
 *
 *  가중치를 "바이트 패킹"으로 저장해 DTCM 32KB에 수납한다.
 *  (8x8 때처럼 int8 하나를 int32 한 워드에 넣으면 25,088개 = 100KB로 초과)
 *  NPU 커널(npu_src/mnist28.c)이 lw + 시프트로 언팩하므로
 *  TCM의 바이트 단위 쓰기 지원 여부와 무관하게 동작한다.
 *
 *  검증 : unicorn 에뮬레이터에서 numpy 정수 시뮬과 로짓까지 비트 단위 일치 확인
 */
#include "xil_printf.h"
#include "xil_io.h"
#include "prog28.h"
#include "weights28.h"
#include "demo28.h"
#include "expected28.h"

/* 주소 맵 — 03_sw/main_mnist.c 와 동일 (2026-07-23 검증) */
#define CB   0x0000000500000000ULL
#define ITCM (CB + 0x00000ULL)
#define DTCM (CB + 0x10000ULL)
#define CSR  (CB + 0x30000ULL)
#define CTRL (CSR + 0x0ULL)
#define PCR  (CSR + 0x4ULL)

/* DTCM 바이트 오프셋 (커널과 일치해야 함) */
#define O_IN   0x0000
#define O_W1   0x0320
#define O_B1   0x6520
#define O_W2   0x65A0
#define O_B2   0x66E0
#define O_OUT  0x6708
#define O_PRED 0x6730
#define O_DONE 0x6734

static void put_bytes(u32 off, const signed char *a, int n)
{   /* int8 4개 -> u32 워드 (AXI는 워드 단위로만 접근) */
    for (int i = 0; i < n; i += 4) {
        u32 w = 0;
        for (int k = 0; k < 4 && i+k < n; k++) w |= ((u32)(unsigned char)a[i+k]) << (8*k);
        Xil_Out32(DTCM + off + i, w);
    }
}
static void put_words(unsigned off, const int *a, int n)
{   for (int i = 0; i < n; i++) Xil_Out32(DTCM + off + i*4, (u32)a[i]); }

static int infer(const unsigned char *img784)
{
    put_bytes(O_IN, (const signed char *)img784, 784);
    Xil_Out32(DTCM + O_DONE, 0);
    Xil_Out32(PCR, 0x0);
    Xil_Out32(CTRL, 0x1);           /* reset */
    Xil_Out32(CTRL, 0x0);           /* run   */
    for (volatile int k = 0; k < 40000000; k++) {
        if (Xil_In32(DTCM + O_DONE)) return (int)Xil_In32(DTCM + O_PRED);
    }
    return -1;                       /* timeout */
}

static void draw28(const unsigned char *p)
{
    const char *sh = " .:-=+*#%@";
    for (int r = 0; r < 28; r++) {
        xil_printf("   ");
        for (int c = 0; c < 28; c++) {
            int k = p[r*28+c] * 9 / 127; if (k > 9) k = 9;
            xil_printf("%c%c", sh[k], sh[k]);
        }
        xil_printf("\r\n");
    }
}

int main(void)
{
    xil_printf("\r\n===== MNIST 28x28 on Coral NPU (FC 784-32-10 int8) =====\r\n");
    xil_printf("weights byte-packed 25.4KB / DTCM 32KB   int8 acc %d/%d (emulator)\r\n\r\n",
               EXPECT_CORRECT_28, N_TEST_28);

    /* 프로그램 + 가중치 적재 (리셋 유지 상태에서 1회) */
    Xil_Out32(CTRL, 0x1);
    for (u32 i = 0; i < NW28; i++) Xil_Out32(ITCM + i*4, npu_prog28[i]);
    put_bytes(O_W1, qW1_28, 25088);
    put_words(O_B1, qb1_28, 32);
    put_bytes(O_W2, qW2_28, 320);
    put_words(O_B2, qb2_28, 10);

    int correct = 0;
    for (int t = 0; t < 10; t++) {
        int p = infer(&demo_x28[t*784]);
        xil_printf("\r\n"); draw28(&demo_x28[t*784]);
        if (p < 0) { xil_printf("   [%d] TIMEOUT\r\n", t); continue; }
        xil_printf("   NPU -> %d   (label %d)  %s\r\n", p, demo_y28[t], p == demo_y28[t] ? "OK" : "X");
        if (p == demo_y28[t]) correct++;
    }
    xil_printf("\r\n===== %d / 10 correct =====\r\n=== done ===\r\n", correct);
    return correct == 10 ? 0 : 1;
}
