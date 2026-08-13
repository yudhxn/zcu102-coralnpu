/* npu_src/mnist.c 와 동일한 정수 연산 (주소만 호스트 배열로) */
int g_dtcm[16384];
#define DT   (g_dtcm)
#define IN   (DT + 0)
#define W1   (DT + 64)
#define B1   (DT + 2112)
#define W2   (DT + 2144)
#define B2   (DT + 2464)
#define OUT  (DT + 2474)
#define PRED (DT + 2484)
#define DONE (DT + 2485)
#define MULT1  1792
#define SHIFT1 20
static int h[32];
void npu_kernel(void)
{
    for (int i = 0; i < 32; i++) {
        int a = B1[i];
        for (int j = 0; j < 64; j++) a += W1[j*32 + i] * IN[j];
        if (a < 0) a = 0;
        a = (a * MULT1) >> SHIFT1;
        if (a > 127) a = 127;
        h[i] = a;
    }
    int best = 0, bv = -2147483647;
    for (int i = 0; i < 10; i++) {
        int a = B2[i];
        for (int j = 0; j < 32; j++) a += W2[j*10 + i] * h[j];
        OUT[i] = a;
        if (a > bv) { bv = a; best = i; }
    }
    PRED[0] = best;
    DONE[0] = 1;
}
