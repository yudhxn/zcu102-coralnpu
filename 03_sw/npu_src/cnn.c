/* 8x8 손글씨 CNN: Conv(8ch 3x3) -> ReLU -> MaxPool(2x2) -> FC(10)
 * 전부 정수 연산. NPU(rv32im)에서 실행.
 * DTCM word 오프셋:
 *   IN    0    : 64   (8x8 입력)
 *   W1    64   : 72   (8ch x 3x3)
 *   B1    136  : 8
 *   W2    144  : 720  (72 x 10)
 *   B2    864  : 10
 *   OUT   874  : 10
 *   PRED  884  : 1
 *   DONE  885  : 1
 */
#define DT   ((volatile int*)0x10000)
#define IN   (DT+0)
#define W1   (DT+64)
#define B1   (DT+136)
#define W2   (DT+144)
#define B2   (DT+864)
#define OUT  (DT+874)
#define PRED (DT+884)
#define DONE (DT+885)

#define MULT  2159
#define SHIFT 20

static int pooled[8*3*3];      /* 8ch x 3x3 */

void cnn_main(void)
{
    /* Conv + ReLU + 재양자화 + MaxPool 을 한 번에 */
    for (int c = 0; c < 8; c++) {
        for (int pi = 0; pi < 3; pi++) {
            for (int pj = 0; pj < 3; pj++) {
                int best = 0;                      /* maxpool 2x2 */
                for (int di = 0; di < 2; di++) {
                    for (int dj = 0; dj < 2; dj++) {
                        int i = pi*2 + di, j = pj*2 + dj;   /* conv 출력 좌표 (6x6) */
                        int a = B1[c];
                        for (int ki = 0; ki < 3; ki++)
                            for (int kj = 0; kj < 3; kj++)
                                a += W1[c*9 + ki*3 + kj] * IN[(i+ki)*8 + (j+kj)];
                        if (a < 0) a = 0;                  /* ReLU */
                        a = (a * MULT) >> SHIFT;           /* 재양자화 */
                        if (a > 127) a = 127;
                        if (a > best) best = a;            /* max */
                    }
                }
                pooled[c*9 + pi*3 + pj] = best;
            }
        }
    }
    /* FC(72 -> 10) + argmax */
    int bi = 0, bv = -2147483647;
    for (int o = 0; o < 10; o++) {
        int a = B2[o];
        for (int k = 0; k < 72; k++) a += W2[k*10 + o] * pooled[k];
        OUT[o] = a;
        if (a > bv) { bv = a; bi = o; }
    }
    PRED[0] = bi;
    DONE[0] = 1;
    for (;;);
}
