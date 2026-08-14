/* coral_mnist28 — 28x28 MNIST (FC 784-32-10 int8)를 실제 Coral NPU에서 추론
 *
 *   ./coral_mnist28              데모 10장 (0~9) + 아스키 아트
 *   ./coral_mnist28 --full DIR   DIR의 t10k_x.bin/t10k_y.bin 전수(10,000장) 평가
 *                                기대값(9,641/10,000)과 다르면 종료코드 1 → CI 검증용
 *
 * 가중치는 바이트 패킹으로 DTCM 32KB에 수납 (int32 저장 방식이면 100KB라 불가).
 * NPU 커널(mnist28.c)은 lw+시프트로 언팩 — TCM 바이트 레인 비의존.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "linux_shim/xil_io.h"
#include "../npu/prog28.h"
#include "weights28.h"
#include "demo28.h"
#include "expected28.h"

#define ITCM (CB_PHYS + 0x00000ULL)
#define DTCM (CB_PHYS + 0x10000ULL)
#define CSR  (CB_PHYS + 0x30000ULL)
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

static void put_bytes(unsigned off, const signed char *a, int n)
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
    /* ★ 2026-08-13 수정: 예전에는 곧바로 nanosleep(1ms)로 잠들었다.
     *   그러면 실제 추론 지연이 1ms 미만이어도 1ms 단위로 반올림되고,
     *   더 나쁘게는 clock()으로 시간을 재면 자는 동안 CPU 시간이 안 쌓여
     *   "장당 0.05ms" 같은 물리적으로 불가능한 수치가 나온다.
     *   먼저 충분히 바쁜 대기를 돌아 실제 지연을 측정할 수 있게 한다. */
    for (long i = 0; i < 20000000L; i++) {
        if (Xil_In32(DTCM + O_DONE)) return (int)Xil_In32(DTCM + O_PRED);
    }
    for (int ms = 0; ms < 2000; ms++) {          /* 그래도 안 끝나면 양보 */
        if (Xil_In32(DTCM + O_DONE)) return (int)Xil_In32(DTCM + O_PRED);
        struct timespec ts = {0, 1000000}; nanosleep(&ts, 0);
    }
    return -1;                       /* timeout */
}

static void draw28(const unsigned char *p)
{
    const char *sh = " .:-=+*#%@";
    for (int r = 0; r < 28; r++) {
        printf("   ");
        for (int c = 0; c < 28; c++) {
            int k = p[r*28+c] * 9 / 127; if (k > 9) k = 9;
            printf("%c%c", sh[k], sh[k]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    printf("\n===== MNIST 28x28 on Coral NPU (FC 784-32-10 int8) =====\n");
    printf("weights byte-packed: 25.4KB / DTCM 32KB   int8 acc %d/%d (emu)\n\n",
           EXPECT_CORRECT_28, N_TEST_28);

    /* 프로그램 + 가중치 적재 (리셋 유지 상태에서 1회) */
    Xil_Out32(CTRL, 0x1);
    for (unsigned i = 0; i < NW28; i++) Xil_Out32(ITCM + i*4, npu_prog28[i]);
    put_bytes(O_W1, qW1_28, 25088);
    put_words(O_B1, qb1_28, 32);
    put_bytes(O_W2, qW2_28, 320);
    put_words(O_B2, qb2_28, 10);

    if (argc > 2 && !strcmp(argv[1], "--full")) {
        char px[512], py[512];
        snprintf(px, sizeof px, "%s/t10k_x.bin", argv[2]);
        snprintf(py, sizeof py, "%s/t10k_y.bin", argv[2]);
        FILE *fx = fopen(px, "rb"), *fy = fopen(py, "rb");
        if (!fx || !fy) { fprintf(stderr, "t10k 파일 없음: %s\n", argv[2]); return 2; }
        static unsigned char X[784]; int correct = 0, n = 0; int y;
        /* ★ clock()은 CPU 시간이라 대기 구간이 빠진다. 실제 경과시간을 쓴다. */
        struct timespec T0, T1; clock_gettime(CLOCK_MONOTONIC, &T0);
        while (fread(X, 1, 784, fx) == 784 && (y = fgetc(fy)) != EOF) {
            int p = infer(X);
            if (p == y) correct++;
            n++;
            if (n % 1000 == 0) { printf("  %5d / %d ...  acc %.4f\n", n, N_TEST_28, (double)correct/n); fflush(stdout); }
        }
        clock_gettime(CLOCK_MONOTONIC, &T1);
        double el = (T1.tv_sec - T0.tv_sec) + (T1.tv_nsec - T0.tv_nsec)/1e9;
        fclose(fx); fclose(fy);
        printf("\n===== full: %d / %d correct (%.2f%%)  [%.1fs, %.2fms/장] =====\n",
               correct, n, 100.0*correct/n, el, 1000.0*el/(n?n:1));
        printf("expected(emulator): %d / %d\n", EXPECT_CORRECT_28, N_TEST_28);
        if (n == N_TEST_28 && correct == EXPECT_CORRECT_28) { printf("== HW/EMU 완전 일치 — PASS ==\n"); return 0; }
        printf("== 불일치 — FAIL ==\n"); return 1;
    }

    int correct = 0;
    for (int t = 0; t < 10; t++) {
        int p = infer(&demo_x28[t*784]);
        printf("\n"); draw28(&demo_x28[t*784]);
        if (p < 0) { printf("   [%d] TIMEOUT\n", t); continue; }
        printf("   NPU -> %d   (label %d)  %s\n", p, demo_y28[t], p == demo_y28[t] ? "OK" : "X");
        if (p == demo_y28[t]) correct++;
    }
    printf("\n===== %d / 10 correct =====\n=== done ===\n", correct);
    return correct == 10 ? 0 : 1;
}
