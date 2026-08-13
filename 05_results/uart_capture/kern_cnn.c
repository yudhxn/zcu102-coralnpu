/* npu_src/cnn.c 와 동일한 정수 연산 */
int g_dtcm[16384];
#define DT   (g_dtcm)
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
static int pooled[8*3*3];
void npu_kernel(void)
{
    for (int c = 0; c < 8; c++)
      for (int pi = 0; pi < 3; pi++)
        for (int pj = 0; pj < 3; pj++) {
            int best = 0;
            for (int di = 0; di < 2; di++)
              for (int dj = 0; dj < 2; dj++) {
                int i = pi*2 + di, j = pj*2 + dj;
                int a = B1[c];
                for (int ki = 0; ki < 3; ki++)
                  for (int kj = 0; kj < 3; kj++)
                    a += W1[c*9 + ki*3 + kj] * IN[(i+ki)*8 + (j+kj)];
                if (a < 0) a = 0;
                a = (a * MULT) >> SHIFT;
                if (a > 127) a = 127;
                if (a > best) best = a;
              }
            pooled[c*9 + pi*3 + pj] = best;
        }
    int bi = 0, bv = -2147483647;
    for (int o = 0; o < 10; o++) {
        int a = B2[o];
        for (int k = 0; k < 72; k++) a += W2[k*10 + o] * pooled[k];
        OUT[o] = a;
        if (a > bv) { bv = a; bi = o; }
    }
    PRED[0] = bi;
    DONE[0] = 1;
}
