/* mnist28 전용: DTCM 데이터가 0x16738까지 차므로 bss/스택을 그 위로 */
ENTRY(_start)
MEMORY { ITCM (rx) : ORIGIN = 0x0,     LENGTH = 8K
         SCR  (rw) : ORIGIN = 0x16800, LENGTH = 0x1700 }
SECTIONS {
  .text : { *(.text.init) *(.text*) *(.rodata*) } > ITCM
  .bss (NOLOAD) : { . = ALIGN(4); *(.bss*) *(COMMON) } > SCR
}
