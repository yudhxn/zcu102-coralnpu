/* wrtest — 쓰기 경로 원인 규명용 최소 실험 (모드별로 한 가지만 시험)
 *
 * 배경 (2026-08-13):
 *   리눅스에서 0x5_0000_0000 읽기는 정상(ITCM 8KB / DTCM 32KB 경계까지 사양과 일치),
 *   그런데 coral_probe(쓰기 포함)를 돌리면 SError -> 커널 패닉.
 *   ARM에서 장치 메모리 쓰기 실패는 비동기라 프로그램이 잡을 수 없다.
 *   따라서 "한 번에 한 가지만" 시험해 어느 접근이 범인인지 좁힌다.
 *
 * 사용:  ./wrtest <모드>
 *   1  DTCM 에 32bit 한 워드 쓰고 읽기        (가장 안전할 것으로 예상)
 *   2  ITCM 에 32bit 한 워드 쓰고 읽기
 *   3  DTCM 에 128bit(4워드) 연속 쓰기        (좁은 폭 문제 확인용)
 *   4  CSR(0x30000) 읽기만                    (읽기 미지원 여부)
 *   5  CSR(0x30000) 에 쓰기                   (가장 위험 — 마지막에)
 *
 * 각 단계 전에 무조건 화면에 찍고 flush 하므로, 패닉이 나도
 * 어디까지 갔는지 시리얼 로그에 남는다.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>

#define CB 0x0000000500000000ULL

static sigjmp_buf jb;
static void on_sig(int s){ (void)s; siglongjmp(jb, 1); }
static void say(const char *m){ printf("%s\n", m); fflush(stdout); }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int mode = (argc > 1) ? atoi(argv[1]) : 1;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }
    signal(SIGBUS, on_sig);
    signal(SIGSEGV, on_sig);

    volatile uint32_t *p = mmap(NULL, 0x40000, PROT_READ|PROT_WRITE,
                                MAP_SHARED, fd, (off_t)CB);
    if (p == MAP_FAILED) { printf("mmap 실패: %s\n", strerror(errno)); return 1; }
    say("mmap OK");

    uint32_t v;
    switch (mode) {
    case 1:
        say("[모드1] DTCM(+0x10000) 32bit 쓰기 시작");
        p[0x10000/4] = 0xA5A5A5A5;
        say("[모드1] 쓰기 통과 (SError 없음)");
        __asm__ volatile("dsb sy" ::: "memory");
        say("[모드1] dsb 통과");
        v = p[0x10000/4];
        printf("[모드1] 읽기값 = 0x%08X  %s\n", v,
               v == 0xA5A5A5A5 ? "일치 — 쓰기 정상" : "불일치");
        break;

    case 2:
        say("[모드2] ITCM(+0x0) 32bit 쓰기 시작");
        p[0] = 0x5A5A5A5A;
        say("[모드2] 쓰기 통과");
        __asm__ volatile("dsb sy" ::: "memory");
        v = p[0];
        printf("[모드2] 읽기값 = 0x%08X  %s\n", v,
               v == 0x5A5A5A5A ? "일치" : "불일치");
        break;

    case 3:
        say("[모드3] DTCM 128bit(4워드) 연속 쓰기 시작");
        p[0x10010/4] = 0x11111111; p[0x10014/4] = 0x22222222;
        p[0x10018/4] = 0x33333333; p[0x1001C/4] = 0x44444444;
        say("[모드3] 쓰기 통과");
        __asm__ volatile("dsb sy" ::: "memory");
        printf("[모드3] 읽기 = %08X %08X %08X %08X\n",
               p[0x10010/4], p[0x10014/4], p[0x10018/4], p[0x1001C/4]);
        break;

    case 4:
        say("[모드4] CSR(+0x30000) 읽기만 시도");
        if (!sigsetjmp(jb, 1)) {
            v = p[0x30000/4];
            printf("[모드4] CSR 읽기값 = 0x%08X\n", v);
        } else {
            say("[모드4] BUS ERROR — CSR은 읽기 미지원");
        }
        break;

    case 5:
        say("[모드5] CSR(+0x30000) 쓰기 시도 — 가장 위험");
        p[0x30000/4] = 0x00000000;
        say("[모드5] 쓰기 통과");
        __asm__ volatile("dsb sy" ::: "memory");
        say("[모드5] dsb 통과 — CSR 쓰기 정상");
        break;

    default:
        say("모드는 1~5");
    }

    munmap((void*)p, 0x40000);
    say("=== 정상 종료 ===");
    return 0;
}
