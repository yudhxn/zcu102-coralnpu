/* coral_probe — PS→PL AXI 창 탐침 + PL 클럭 설정 (리눅스, 정적 바이너리)
 *
 * 용도:
 *   ./coral_probe            : 후보 주소 창에서 ITCM/DTCM write→read 테스트
 *   ./coral_probe --clk50    : PL0_REF_CTRL 을 50MHz(div0=30,div1=1)로 설정
 *   ./coral_probe --clkshow  : 현재 PL0~PL3 레지스터 출력
 *
 * 배경: 프리빌트(2019.1 TRD) FSBL의 psu_init은 우리 XSA와 다르므로
 *       Coral 창(0x5_0000_0000)이 정말 열려있는지 실측해야 한다.
 *       SIGBUS를 잡아 죽지 않고 다음 후보로 넘어간다.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>

static sigjmp_buf jb;
static void on_bus(int s){ (void)s; siglongjmp(jb, 1); }

static int test_window(int fd, uint64_t phys, const char *name)
{
    volatile uint32_t *p = mmap(NULL, 0x40000, PROT_READ|PROT_WRITE,
                                MAP_SHARED, fd, (off_t)phys);
    if (p == MAP_FAILED) { printf("  %-14s mmap 실패\n", name); return 0; }
    int ok = 0;
    if (!sigsetjmp(jb, 1)) {
        static const uint32_t pat[4] = {0xA5A5A5A5, 0x5A5A5A5A, 0x12345678, 0xCAFEBABE};
        int good = 1;
        for (int r = 0; r < 2; r++) {            /* r=0: ITCM(+0), r=1: DTCM(+0x10000) */
            uint32_t off = r ? 0x10000/4 : 0;
            for (int i = 0; i < 4; i++) p[off+i] = pat[i];
            for (int i = 0; i < 4; i++) if (p[off+i] != pat[i]) good = 0;
        }
        uint32_t st = p[0x30008/4];              /* CSR status */
        printf("  %-14s ITCM/DTCM write-read %s   CSR status=0x%08X\n",
               name, good ? "PASS" : "MISMATCH", st);
        ok = good;
    } else {
        printf("  %-14s BUS ERROR (창 닫힘)\n", name);
    }
    munmap((void*)p, 0x40000);
    return ok;
}

#define CRL_APB 0xFF5E0000ULL
static void clk(int fd, int set50)
{
    volatile uint32_t *c = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE,
                                MAP_SHARED, fd, (off_t)CRL_APB);
    if (c == MAP_FAILED) { perror("mmap CRL_APB"); return; }
    for (int i = 0; i < 4; i++) {
        uint32_t v = c[(0xC0 + 4*i)/4];
        printf("  PL%d_REF_CTRL = 0x%08X  (src=%u div0=%u div1=%u act=%u)\n",
               i, v, v & 7, (v >> 8) & 0x3F, (v >> 16) & 0x3F, (v >> 24) & 1);
    }
    if (set50) {
        uint32_t v = c[0xC0/4];
        uint32_t nv = (v & ~0x003F3F00u) | (30u << 8) | (1u << 16);  /* div0=30 div1=1 */
        c[0xC0/4] = nv;
        printf("  PL0_REF_CTRL: 0x%08X -> 0x%08X (IOPLL 1500/30 = 50MHz 기준)\n", v, c[0xC0/4]);
        printf("  ※ FSBL이 IOPLL이 아닌 다른 소스를 쓰면 위 div0 값 조정 필요\n");
    }
    munmap((void*)c, 0x1000);
}

int main(int argc, char **argv)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem (root로 실행)"); return 1; }
    signal(SIGBUS, on_bus);

    if (argc > 1 && !strcmp(argv[1], "--clkshow")) { clk(fd, 0); return 0; }
    if (argc > 1 && !strcmp(argv[1], "--clk50"))   { clk(fd, 1); return 0; }

    printf("Coral AXI 창 탐침 (비트스트림 로드 후 실행할 것)\n");
    struct { uint64_t a; const char *n; } w[] = {
        {0x0000000500000000ULL, "0x5_0000_0000"},   /* 우리 설계의 주소 */
        {0x0000000400000000ULL, "0x4_0000_0000"},   /* HPM0 기본 고영역 */
        {0x00000000A0000000ULL, "0xA000_0000"},     /* HPM0 저영역 */
        {0x00000000B0000000ULL, "0xB000_0000"},     /* HPM1 저영역 */
    };
    int hit = -1;
    for (unsigned i = 0; i < sizeof(w)/sizeof(w[0]); i++)
        if (test_window(fd, w[i].a, w[i].n) && hit < 0) hit = (int)i;
    if (hit == 0)      printf("\n결론: 0x5_0000_0000 정상 — 로더 그대로 사용 가능\n");
    else if (hit > 0)  printf("\n결론: %s 에서 응답 — 로더의 CB_PHYS를 이 값으로 수정 필요\n", w[hit].n);
    else               printf("\n결론: 응답 없음 — 비트스트림 로드 여부/psu_init(HPM 설정) 확인\n");
    return hit == 0 ? 0 : 2;
}
