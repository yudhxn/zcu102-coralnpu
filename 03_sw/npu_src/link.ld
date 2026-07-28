ENTRY(_start)
MEMORY { ITCM (rx) : ORIGIN = 0x0,     LENGTH = 8K
         SCR  (rw) : ORIGIN = 0x14000, LENGTH = 16K }
SECTIONS {
  .text : { *(.text.init) *(.text*) *(.rodata*) } > ITCM
  .bss (NOLOAD) : { . = ALIGN(4); *(.bss*) *(COMMON) } > SCR
}
