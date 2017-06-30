MEMORY
{
  /* Mappable IRAM, 32k@0x40108000, normally used by the flash cache.
   * Cache_Read_Enable() can be instructed to only use the upper 16k however,
   * which allows the boot loader to sit in the lower 16k.
   * To maximise the chance of being able to chain-load another boot
   * loader without overwriting ourselves, we load as high as possible.
   */
  miram1_0_seg :                        org = 0x4010bc00, len = 0x00400 
  /* For esptool.py compatibility *only* - no irom is used by the boot loader */
  irom0_0_seg  :                        org = 0x40201000, len = 0x0f000
  dram0_0_seg  :                        org = 0x3ffe8000, len = 0x14000
  /* Location of ets_printf() control flags, see .data section below. */
  printf_seg   :                        org = 0x3fffdd48, len = 0x00008
}

PHDRS
{
  dram0_0_phdr PT_LOAD;
  dram0_0_bss_phdr PT_LOAD;
  miram1_0_phdr PT_LOAD;
  irom0_0_phdr PT_LOAD;
  printf_phdr  PT_LOAD;
}

ENTRY(boot_main)

SECTIONS
{
  /* The boot loader does not make use of a traditional .data section.
   * However, in a non-VERBOSE build, we use the .data section to clear
   * the flags controlling ets_printf behaviour. In doing so, we
   * essentially get the ROM loader to silence itself, speeding up
   * bootup time by a few ms and thus saving a small amount of power. */
  .data : ALIGN(4)
  {
    _data_start = ABSOLUTE(.);
    *(.rodata.disable_ets_printf)
  } >printf_seg :printf_phdr

  
  /* In a VERBOSE build, ets_printf() strings, or otherwise empty.
   * For alignment reasons we cache these into RAM. */
  .rodata : ALIGN(4)
  {
    _rodata_start = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
  } >dram0_0_seg :dram0_0_phdr


  .text : ALIGN(4)
  {
    _text_start = ABSOLUTE(.);
    *(.text)
    *(.literal)
    *(.text.*)
    *(.literal.*)
  } >miram1_0_seg :miram1_0_phdr


  /* For esptool.py compatibility *only* - no irom is used by the boot loader */
  .irom0.text : ALIGN(4)
  {
    _irom0_text_start = ABSOLUTE(.);
  } >irom0_0_seg :irom0_0_phdr

  call_user_start = boot_main; /* To make "esptool.py elf2image" work */
}

PROVIDE ( uart_div_modify = 0x400039d8 );
INCLUDE "eagle.rom.addr.v6.ld"
