#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"

/* =====================================================================
 *  LeNet-5 (IREE 런타임 포함) ELF 탑재 가능성 검증
 *  대상: Coral 에뮬레이터에서 동작하던 lenet5_coral.elf
 *
 *  [정적 분석]
 *    .text      533,728 B  vs  ITCM    8,192 B  ->  65배 초과
 *    전체     1,684,320 B  vs  전체   40,960 B  ->  41배 초과
 *    스택 주소  0x200000(2MB)  ->  DTCM(0x10000~0x18000) 범위 밖
 *    벡터 명령  0개  ->  ISA 자체는 호환
 *
 *  이 테스트는 ELF 코드 일부를 실제 ITCM에 써서
 *  "적재 경로는 정상이나 용량이 절대 부족"함을 하드웨어에서 확인한다.
 * ===================================================================== */
#define CB   0x0000000500000000ULL
#define ITCM (CB + 0x00000ULL)
#define CSR  (CB + 0x30000ULL)
#define CTRL (CSR + 0x0ULL)

#define TEXT_TOTAL   533728
#define ITCM_BYTES     8192
#define TOTAL_NEED  1684320
#define TOTAL_HAVE    40960

static const u32 lenet_sample[256] = {
    0xB0205073u, 0xB8205073u, 0xB0202573u, 0xB82025F3u, 0x00200117u, 0xFF010113u, 0x00100197u, 0x7E818193u,
    0x00000213u, 0x00000313u, 0x00000393u, 0x00000413u, 0x00000493u, 0x00000593u, 0x00000613u, 0x00000693u,
    0x00000713u, 0x00000793u, 0x00000813u, 0x00000893u, 0x00000913u, 0x00000993u, 0x00000A13u, 0x00000A93u,
    0x00000B13u, 0x00000B93u, 0x00000C13u, 0x00000C93u, 0x00000D13u, 0x00000D93u, 0x00000E13u, 0x00000E93u,
    0x00000F13u, 0x00000F93u, 0x00101517u, 0x37850513u, 0x00101597u, 0x6C458593u, 0x448820EFu, 0x00082417u,
    0x4B440413u, 0x00082497u, 0x4AC48493u, 0x00947A63u, 0x00042283u, 0x000280E7u, 0x00440413u, 0xFE946AE3u,
    0x00038297u, 0xC6028293u, 0x30529073u, 0x000062B7u, 0x60028293u, 0x3002A073u, 0x00101297u, 0x30028293u,
    0x0BADD537u, 0x00D50513u, 0x00A2A023u, 0x00000513u, 0x00000593u, 0x00000097u, 0x06408093u, 0x000080E7u,
    0x00050913u, 0x739370EFu, 0x00082417u, 0x44840413u, 0x00082497u, 0x44048493u, 0x00940A63u, 0xFFC48493u,
    0x0004A283u, 0x000280E7u, 0xFE941AE3u, 0x00090513u, 0x00101297u, 0x2A828293u, 0x00A2A023u, 0x00050663u,
    0x00100073u, 0x0100006Fu, 0xB0202573u, 0xB82025F3u, 0x08000073u, 0x0000006Fu, 0xF5010113u, 0x0A112623u,
    0x0A812423u, 0x0A912223u, 0x0B212023u, 0x09312E23u, 0x09412C23u, 0x09512A23u, 0x09612823u, 0xFFF00593u,
    0x48B1A023u, 0x00101AB7u, 0x420AA023u, 0x06012223u, 0x06012023u, 0x04012E23u, 0x04012C23u, 0x04012A23u,
    0x04012823u, 0x04012623u, 0x04012423u, 0x538000EFu, 0x00050913u, 0x01452503u, 0x000015B7u, 0xC4058593u,
    0x3EB51863u, 0x02092503u, 0x02800593u, 0x3EB51263u, 0x01092583u, 0x00100537u, 0x04050513u, 0x00001637u,
    0xC4060613u, 0x270750EFu, 0x01C92583u, 0x4C018413u, 0x04040493u, 0x02800613u, 0x00048513u, 0x258750EFu,
    0x00101A37u, 0x00100513u, 0x40AA2E23u, 0x0005B637u, 0x1B060613u, 0x02000513u, 0x06410693u, 0x00000593u,
    0x385480EFu, 0x4A051263u, 0x00200513u, 0x40AA2E23u, 0x06412503u, 0x304390EFu, 0x48051863u, 0x00093537u,
    0xD1750513u, 0x0005B6B7u, 0x1B068693u, 0x00C00593u, 0x06010813u, 0x00000613u, 0x00000713u, 0x00068793u,
    0x0143A0EFu, 0x46051263u, 0x06412983u, 0x03810513u, 0x7A1370EFu, 0x06012683u, 0x0005B7B7u, 0x1B078793u,
    0x03810613u, 0x05C10813u, 0x00098513u, 0x00000593u, 0x00000713u, 0x45C000EFu, 0x42051863u, 0x00101A37u,
    0x00300513u, 0x40AA2E23u, 0x06412503u, 0x00492603u, 0x00892683u, 0x05810593u, 0x0005B8B7u, 0x1B088893u,
    0x00B12023u, 0x00000593u, 0x00000713u, 0x00000793u, 0x00000813u, 0x3A4010EFu, 0x3E051863u, 0x00400513u,
    0x40AA2E23u, 0x05C12583u, 0x05812603u, 0x06412503u, 0x02B12823u, 0x02C12A23u, 0x0005B7B7u, 0x1B078793u,
    0x00200613u, 0x03010693u, 0x05410813u, 0x00000593u, 0x00000713u, 0x7F4470EFu, 0x3A051863u, 0x05412503u,
    0x000935B7u, 0xD2458593u, 0x00B00613u, 0x02810693u, 0x6E4480EFu, 0x38051A63u, 0x00101537u, 0x00500593u,
    0x40B52E23u, 0x06012903u, 0x00012C23u, 0x0009A5B7u, 0x8B858593u, 0x06810513u, 0x02000613u, 0x0F8750EFu,
    0x00001537u, 0x06810593u, 0xC4050613u, 0x01810693u, 0x00090513u, 0x455390EFu, 0x34051863u, 0x01812503u,
    0x00100593u, 0x00200613u, 0xFFF00713u, 0x06810793u, 0x00100A13u, 0x00000693u, 0x7B13A0EFu, 0x32051C63u,
    0x06812503u, 0x001005B7u, 0x04058593u, 0x00001637u, 0xC4060613u, 0x0A0750EFu, 0x06810513u, 0x0443B0EFu,
    0x01812903u, 0x01C00513u, 0x01412C23u, 0x01412E23u, 0x02A12023u, 0x02A12223u, 0x0005B837u, 0x1B080813u,
    0x21000537u, 0x00400593u, 0x01810613u, 0x00100713u, 0x02050693u, 0x05010893u, 0x00090513u, 0x00000793u,
};

#define NS (sizeof(lenet_sample)/sizeof(lenet_sample[0]))

int main(void)
{
    xil_printf("\r\n===== LeNet-5 (IREE) ELF load test on Coral NPU =====\r\n");

    xil_printf("\r\n[static analysis]\r\n");
    xil_printf("  .text size    : %d bytes\r\n", TEXT_TOTAL);
    xil_printf("  ITCM capacity : %d bytes\r\n", ITCM_BYTES);
    xil_printf("  -> code fits  : %d %% (65x over capacity)\r\n",
               (ITCM_BYTES*100)/TEXT_TOTAL);
    xil_printf("  total needed  : %d bytes\r\n", TOTAL_NEED);
    xil_printf("  total avail   : %d bytes (ITCM+DTCM)\r\n", TOTAL_HAVE);
    xil_printf("  -> 41x over available memory\r\n");
    xil_printf("  stack at      : 0x200000 = outside DTCM range\r\n");
    xil_printf("  vector insns  : 0 -> ISA compatible\r\n");

    xil_printf("\r\n[load test] writing %d words of LeNet .text to ITCM...\r\n", (int)NS);
    Xil_Out32(CTRL, 0x1);
    for (u32 i = 0; i < NS; i++) Xil_Out32(ITCM + i*4, lenet_sample[i]);

    u32 ok = 0;
    for (u32 i = 0; i < NS; i++)
        if (Xil_In32(ITCM + i*4) == lenet_sample[i]) ok++;

    xil_printf("[load test] verified %d / %d words\r\n", (int)ok, (int)NS);
    if (ok == NS)
        xil_printf("  -> AXI load path WORKS. Problem is capacity, not access.\r\n");
    else
        xil_printf("  -> load mismatch\r\n");

    xil_printf("\r\n===== CONCLUSION =====\r\n");
    xil_printf("The ELF cannot run here: it needs 1.6 MB but this NPU\r\n");
    xil_printf("configuration provides 40 KB (ITCM 8KB + DTCM 32KB).\r\n");
    xil_printf("Most of the size is the IREE runtime (VM interpreter),\r\n");
    xil_printf("not the model itself. The emulator has no memory limit.\r\n");
    xil_printf("Options: (a) rebuild NPU with highmem (1MB TCM), or\r\n");
    xil_printf("         (b) native reimplementation without IREE.\r\n");
    xil_printf("Our native models run the same task in 440 bytes.\r\n");
    xil_printf("=== done ===\r\n");
    return 0;
}
