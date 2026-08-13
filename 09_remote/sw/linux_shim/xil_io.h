/* 리눅스 /dev/mem 버전 Xil_Out32/Xil_In32 — 베어메탈 소스를 그대로 재사용 */
#ifndef XIL_IO_H
#define XIL_IO_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
typedef uint32_t u32;
typedef uint64_t u64;
#define CB_PHYS 0x0000000500000000ULL
#define CB_SPAN 0x40000ULL
static volatile uint32_t *cb_base;
__attribute__((constructor)) static void cb_init(void){
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem (root 필요)"); exit(1); }
    void *p = mmap(NULL, CB_SPAN, PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)CB_PHYS);
    if (p == MAP_FAILED) { perror("mmap 0x5_0000_0000"); exit(1); }
    cb_base = (volatile uint32_t *)p;
}
static inline void Xil_Out32(u64 a, u32 v){ cb_base[(a - CB_PHYS) >> 2] = v; }
static inline u32  Xil_In32 (u64 a)       { return cb_base[(a - CB_PHYS) >> 2]; }
#endif
