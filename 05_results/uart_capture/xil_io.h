#ifndef XIL_IO_H
#define XIL_IO_H
#include <stdint.h>
typedef uint32_t u32;
typedef uint64_t u64;
#define CB_BASE 0x0000000500000000ULL
#define DTCM_BASE (CB_BASE + 0x10000ULL)
#define ITCM_BASE (CB_BASE + 0x00000ULL)
#define CSR_BASE  (CB_BASE + 0x30000ULL)
#define CTRL_ADDR (CSR_BASE + 0x0ULL)
#define PCR_ADDR  (CSR_BASE + 0x4ULL)
extern int g_dtcm[16384];
void npu_kernel(void);          /* provided per-model */
static inline void Xil_Out32(u64 a, u32 v){
    if (a >= DTCM_BASE && a < DTCM_BASE + 16384*4) { g_dtcm[(a - DTCM_BASE)/4] = (int)v; return; }
    if (a == CTRL_ADDR && v == 0) { npu_kernel(); return; }   /* core released -> run */
}
static inline u32 Xil_In32(u64 a){
    if (a >= DTCM_BASE && a < DTCM_BASE + 16384*4) return (u32)g_dtcm[(a - DTCM_BASE)/4];
    return 0;
}
#endif
