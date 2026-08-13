/* 28x28 MNIST int8 추론 (FC 784 -> 32 ReLU -> 10) — Coral NPU rv32im
 *
 * 8x8 mnist.c의 확장. 차이점:
 *  - 가중치/입력을 "바이트 패킹"으로 저장해 DTCM 32KB에 수납
 *    (int32 한 워드에 int8 4개 — 기존 방식이면 100KB라 불가능)
 *  - TCM 바이트 레인 의존을 피하려고 lb 대신 lw + 시프트/마스크로 언팩
 *  - 누적값(최대 1.2e7) * M1 이 32bit를 넘으므로 재양자화는 64bit 곱
 *
 * DTCM 바이트 오프셋 (NPU 관점 베이스 0x10000):
 *  IN 0x0000(784B) W1 0x0320(25088B, [i*784+j]) B1 0x6520(32 int)
 *  W2 0x65A0(320B, [o*32+m]) B2 0x66E0(10 int) OUT 0x6708(10 int)
 *  PRED 0x6730  DONE 0x6734
 */
#include "params28.h"

#define DT 0x10000u
#define INW   ((const unsigned int *)(DT + 0x0000))
#define W1B   (DT + 0x0320)
#define B1    ((const int *)(DT + 0x6520))
#define W2B   (DT + 0x65A0)
#define B2    ((const int *)(DT + 0x66E0))
#define OUT   ((volatile int *)(DT + 0x6708))
#define PRED  ((volatile int *)(DT + 0x6730))
#define DONE  ((volatile int *)(DT + 0x6734))

static int h[32];

#define SX(w, s)  ((int)(signed char)(((w) >> (s)) & 0xFF))   /* 부호 확장 */
#define UX(w, s)  ((int)(((w) >> (s)) & 0xFF))                /* 0..127    */

void main28(void)
{
    /* layer1: 784 -> 32, ReLU, 재양자화 */
    for (int i = 0; i < 32; i++) {
        int a = B1[i];
        const unsigned int *wp = (const unsigned int *)(W1B + (unsigned)i*784u);
        for (int jw = 0; jw < 196; jw++) {
            unsigned int xw = INW[jw], ww = wp[jw];
            a += SX(ww,0)*UX(xw,0) + SX(ww,8)*UX(xw,8)
               + SX(ww,16)*UX(xw,16) + SX(ww,24)*UX(xw,24);
        }
        if (a < 0) a = 0;
        a = (int)(((long long)a * M1_28) >> SHIFT_28);
        if (a > 127) a = 127;
        h[i] = a;
    }
    /* layer2: 32 -> 10 + argmax */
    int best = 0, bv = -2147483647;
    for (int o = 0; o < 10; o++) {
        int a = B2[o];
        const unsigned int *wp = (const unsigned int *)(W2B + (unsigned)o*32u);
        for (int jw = 0; jw < 8; jw++) {
            unsigned int ww = wp[jw];
            int m = jw*4;
            a += SX(ww,0)*h[m] + SX(ww,8)*h[m+1]
               + SX(ww,16)*h[m+2] + SX(ww,24)*h[m+3];
        }
        OUT[o] = a;
        if (a > bv) { bv = a; best = o; }
    }
    PRED[0] = best;
    DONE[0] = 1;
    for (;;);
}
