/****************************************************************************
 * build/configs/tizens3/scripts/esp32s3.ld
 *
 * TizenRT linker script for the ESP32-S3, loaded by an ESP-IDF v4.4
 * 2nd stage bootloader.
 *
 * Internal SRAM layout used here:
 *
 *   0x40370000-0x40378000  SRAM0, may be claimed as ICache: not used
 *   0x40378000-0x40380000  IRAM: vectors + IRAM_ATTR code (32 KB)
 *                          (same physical RAM as DRAM 0x3fc88000-0x3fc90000)
 *   0x3fc90000-0x3fce9000  DRAM: .data, .bss, idle stack, heap
 *   0x3fcd9700-...         bootloader lives here while loading us, so our
 *                          *loaded* segments must stay below it; the heap
 *                          may grow over it after boot.
 *   0x3fce9000-0x3fd00000  ROM .bss/.data/stack + DCache: not used
 *
 * Flash is mapped through the S3's unified MMU: code at 0x42000000 (IBUS)
 * and rodata at 0x3c000000 (DBUS) share the SAME table entries, so rodata
 * is pushed past the end of .flash.text (the "dummy" section below) to
 * avoid both mapping the same 64 KB page.
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

MEMORY
{
  iram0_0_seg (RX) : org = 0x40378000, len = 0x8000
  dram0_0_seg (RW) : org = 0x3fc90000, len = 0x3fce9000 - 0x3fc90000
  /* 0x20 offset: room for the image + segment headers so that
   * (flash offset % 64KB) == (vaddr % 64KB) works out naturally.
   */
  irom0_0_seg (RX) : org = 0x42000020, len = 0x800000 - 0x20
  drom0_0_seg (R)  : org = 0x3c000020, len = 0x800000 - 0x20
}

/* End of the area TizenRT may use as heap */

_eheap = 0x3fce9000;

ENTRY(__start);

SECTIONS
{
  .iram0.vectors :
  {
    _init_start = ABSOLUTE(.);

    /* LX7 vector offsets are the same as on the ESP32's LX6 */

    . = 0x0;
    KEEP (*(.window_vectors.text));
    . = 0x180;
    KEEP (*(.xtensa_level2_vector.text));
    . = 0x1c0;
    KEEP (*(.xtensa_level3_vector.text));
    . = 0x200;
    KEEP (*(.xtensa_level4_vector.text));
    . = 0x240;
    KEEP (*(.xtensa_level5_vector.text));
    . = 0x280;
    KEEP (*(.debug_exception_vector.text));
    . = 0x2c0;
    KEEP (*(.nmi_vector.text));
    . = 0x300;
    KEEP (*(.kernel_exception_vector.text));
    . = 0x340;
    KEEP (*(.user_exception_vector.text));
    . = 0x3c0;
    KEEP (*(.double_exception_vector.text));
    . = 0x400;
    *(.*_vector.literal)

    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)
    _init_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.text :
  {
    _iram_text_start = ABSOLUTE(.);
    *(.iram1 .iram1.*)
    _iram_text_end = ABSOLUTE(.);
  } > iram0_0_seg

  .dram0.data :
  {
    _sdata = ABSOLUTE(.);
    KEEP (*(.data))
    KEEP (*(.data.*))
    KEEP (*(.gnu.linkonce.d.*))
    KEEP (*(.data1))
    KEEP (*(.sdata))
    KEEP (*(.sdata.*))
    KEEP (*(.gnu.linkonce.s.*))
    KEEP (*(.sdata2))
    KEEP (*(.sdata2.*))
    KEEP (*(.gnu.linkonce.s2.*))
    KEEP (*(.jcr))
    *(.dram1 .dram1.*)
    _edata = ABSOLUTE(.);
  } > dram0_0_seg

  .dram0.bss (NOLOAD) :
  {
    . = ALIGN (8);
    _sbss = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    KEEP (*(.bss))
    *(.bss.*)
    *(.share.mem)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN (8);
    _ebss = ABSOLUTE(.);

    /* Uninitialized: idle stack lives here */
    . = ALIGN (16);
    *(.noinit)
    . = ALIGN (16);
    _sheap = ABSOLUTE(.);
  } > dram0_0_seg

  .flash.text :
  {
    _stext_flash = .;
    _text_start = ABSOLUTE(.);
    *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.irom0.text)
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext_flash = .;
  } > irom0_0_seg

  /* Skip DBUS address space equal to the code size so rodata lands on
   * MMU pages that .flash.text does not use.
   */

  .flash.rodata_dummy (NOLOAD) :
  {
    . = SIZEOF(.flash.text);
    . = ALIGN(0x10000) + 0x20;
  } > drom0_0_seg

  .flash.rodata :
  {
    _srodata = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE_ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table .gcc_except_table.*)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    . = (. + 3) & ~ 3;
    __eh_frame = ABSOLUTE(.);
    KEEP(*(.eh_frame))
    . = (. + 7) & ~ 3;

    _sinit = ABSOLUTE(.);
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    _einit = ABSOLUTE(.);
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))

    __XT_EXCEPTION_DESCS_ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    _erodata = ABSOLUTE(.);
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
    . = ALIGN(4);
  } > drom0_0_seg
}
