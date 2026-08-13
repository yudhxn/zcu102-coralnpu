#include <errno.h>
/* rdprobe — mmap 기반 읽기 전용 탐침 (보드를 죽이지 않는다)
 *
 * 쓰기는 ARM에서 비동기 abort -> SError -> 커널 패닉이지만,
 * 읽기는 동기 abort -> SIGBUS 라서 잡아서 살아남을 수 있다.
 *
 * 사용: ./rdprobe            (기본 후보 주소들을 순회)
 *       ./rdprobe 0x5_...    (특정 물리주소 하나만)
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
static void on_sig(int s){ (void)s; siglongjmp(jb, 1); }

static void rd(int fd, uint64_t phys, const char *name)
{
    long ps = sysconf(_SC_PAGESIZE);
    uint64_t base = phys & ~((uint64_t)ps - 1);
    uint64_t off  = phys - base;
    volatile uint8_t *p = mmap(NULL, ps*2, PROT_READ, MAP_SHARED, fd, (off_t)base);
    if (p == MAP_FAILED) { printf("  %-16s mmap FAIL (%s)\n", name, strerror(errno)); return; }
    if (!sigsetjmp(jb, 1)) {
        volatile uint32_t *w = (volatile uint32_t *)(p + off);
        uint32_t a = w[0], b = w[1], c = w[2], d = w[3];
        printf("  %-16s READ OK  %08X %08X %08X %08X\n", name, a, b, c, d);
    } else {
        printf("  %-16s BUS ERROR (응답 없음)\n", name);
    }
    munmap((void*)p, ps*2);
}

int main(int argc, char **argv)
{
    int fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }
    signal(SIGBUS, on_sig);
    signal(SIGSEGV, on_sig);

    if (argc > 1) {
        uint64_t a = strtoull(argv[1], NULL, 0);
        rd(fd, a, argv[1]);
        return 0;
    }
    printf("읽기 전용 탐침 (쓰기 안 함 — 보드가 죽지 않음)\n");
    rd(fd, 0xFF5E00C0ULL,        "PL0_REF_CTRL");   /* 정상 동작 기준점 */
    rd(fd, 0x00000000FD1A0000ULL,"CRF_APB");        /* 또 하나의 기준점 */
    rd(fd, 0x0000000500000000ULL,"NPU ITCM 0x5_0");
    rd(fd, 0x0000000500010000ULL,"NPU DTCM +0x10000");
    rd(fd, 0x0000000500030000ULL,"NPU CSR  +0x30000");
    rd(fd, 0x0000000400000000ULL,"HPM0 0x4_0");
    rd(fd, 0x00000000A0000000ULL,"HPM0 0xA000_0000");
    rd(fd, 0x00000000B0000000ULL,"HPM1 0xB000_0000");
    return 0;
}
