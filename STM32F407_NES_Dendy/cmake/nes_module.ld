ENTRY(NesModule_Init)
EXTERN(NesModule_RunFrame)

MEMORY
{
  NESRAM (rwx) : ORIGIN = 0x20010000, LENGTH = 63K
  NESCCM (rw)  : ORIGIN = 0x1000A000, LENGTH = 24K
}

PHDRS
{
  program PT_LOAD FLAGS(5); /* Read/execute */
  state PT_LOAD FLAGS(6);   /* Read/write */
  ccm PT_LOAD FLAGS(6);
}

SECTIONS
{
  /* Keep this layout in sync with NesModuleHeader. Function pointers
     include the Thumb bit. The packaging script fills the CRC field. */
  .header :
  {
    LONG(0x3153454E)
    LONG(DEFINED(__nes_module_abi) ? __nes_module_abi : 1)
    LONG(__image_end - ORIGIN(NESRAM))
    LONG(ADDR(.bss) - ORIGIN(NESRAM))
    LONG(SIZEOF(.bss))
    LONG(0)
    LONG(NesModule_Init | 1)
    LONG(NesModule_RunFrame | 1)
  } > NESRAM :program

  .text :
  {
    *(.text .text.*)
    *(.rodata .rodata.*)
  } > NESRAM :program

  .data :
  {
    *(.data .data.*)
    BYTE(0)
    . = ALIGN(4);
    __image_end = .;
  } > NESRAM :state

  .bss (NOLOAD) :
  {
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
  } > NESRAM :state

  .nes_ccm (NOLOAD) :
  {
    *(.nes_ccm .nes_ccm.*)
  } > NESCCM :ccm

  .constructors :
  {
    KEEP(*(.preinit_array* .init_array* .fini_array* .ctors* .dtors*))
  } > NESRAM :state

  /DISCARD/ :
  {
    *(.ARM.exidx* .ARM.extab* .eh_frame*)
  }
}

ASSERT(SIZEOF(.header) == 32, "InfoNES module header must be 32 bytes")
ASSERT((NesModule_Init & ~1) >= ADDR(.text) &&
       (NesModule_Init & ~1) < ADDR(.text) + SIZEOF(.text),
       "InfoNES initialization entry is outside its program")
ASSERT((NesModule_RunFrame & ~1) >= ADDR(.text) &&
       (NesModule_RunFrame & ~1) < ADDR(.text) + SIZEOF(.text),
       "InfoNES frame entry is outside its program")
ASSERT(SIZEOF(.constructors) == 0, "InfoNES module must not use C++ dynamic initialization")
ASSERT(ADDR(.bss) + SIZEOF(.bss) <= ORIGIN(NESRAM) + LENGTH(NESRAM),
       "InfoNES module exceeds its SRAM region")
