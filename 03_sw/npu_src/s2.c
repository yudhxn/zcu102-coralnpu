/* Stride-2 Conv 경량 모델: Conv(8ch 3x3, stride 2) -> ReLU -> FC(10)
 * 풀링 없이 stride로 다운샘플 → 연산량 최소화
 * DTCM word 오프셋: IN 0 / W1 64 / B1 136 / W2 144 / B2 864 / OUT 874 / PRED 884 / DONE 885 */
#define DT   ((volatile int*)0x10000)
#define IN   (DT+0)
#define W1   (DT+64)
#define B1   (DT+136)
#define W2   (DT+144)
#define B2   (DT+864)
#define OUT  (DT+874)
#define PRED (DT+884)
#define DONE (DT+885)
#define MULT  2973
#define SHIFT 20

static int feat[72];      /* 3x3 위치 x 8채널 */

void s2_main(void)
{
    int t = 0;
    for (int i = 0; i < 6; i += 2) {          /* stride 2: i,j = 0,2,4 */
        for (int j = 0; j < 6; j += 2) {
            for (int c = 0; c < 8; c++) {
                int a = B1[c];
                for (int ki = 0; ki < 3; ki++)
                    for (int kj = 0; kj < 3; kj++)
                        a += W1[c*9 + ki*3 + kj] * IN[(i+ki)*8 + (j+kj)];
                if (a < 0) a = 0;
                a = (a * MULT) >> SHIFT;
                if (a > 127) a = 127;
                feat[t*8 + c] = a;
            }
            t++;
        }
    }
    int bi = 0, bv = -2147483647;
    for (int o = 0; o < 10; o++) {
        int a = B2[o];
        for (int k = 0; k < 72; k++) a += W2[k*10 + o] * feat[k];
        OUT[o] = a;
        if (a > bv) { bv = a; bi = o; }
    }
    PRED[0] = bi;
    DONE[0] = 1;
    for (;;);
}
