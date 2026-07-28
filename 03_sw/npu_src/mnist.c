/* MNIST 8x8 손글씨 int8 추론 (64 -> 32 ReLU -> 10)
 * NPU(Coral, rv32im)에서 실행. 모든 연산 정수.
 * DTCM 배치(NPU 관점 주소):
 *   IN   0x0000 : int32 x64   (입력, 0..127로 양자화된 값)
 *   W1   0x0100 : int32 x2048 (64x32)  -> 8KB
 *   B1   0x2100 : int32 x32
 *   W2   0x2180 : int32 x320  (32x10)
 *   B2   0x2680 : int32 x10
 *   OUT  0x26A8 : int32 x10   (로짓)
 *   PRED 0x26D0 : int32       (예측 숫자)
 *   DONE 0x26D4 : int32
 */
#define DT   ((volatile int*)0x10000)
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

void _start(void)
{
    /* layer1: 64 -> 32, ReLU, 재양자화 */
    for (int i = 0; i < 32; i++) {
        int a = B1[i];
        for (int j = 0; j < 64; j++) a += W1[j*32 + i] * IN[j];
        if (a < 0) a = 0;
        a = (a * MULT1) >> SHIFT1;
        if (a > 127) a = 127;
        h[i] = a;
    }
    /* layer2: 32 -> 10, argmax */
    int best = 0, bv = -2147483647;
    for (int i = 0; i < 10; i++) {
        int a = B2[i];
        for (int j = 0; j < 32; j++) a += W2[j*10 + i] * h[j];
        OUT[i] = a;
        if (a > bv) { bv = a; best = i; }
    }
    PRED[0] = best;
    DONE[0] = 1;
    for (;;);
}
