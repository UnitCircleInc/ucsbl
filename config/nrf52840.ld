/*
 * © 2025 Unit Circle Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/*
 * BL_RAM is placed at the end of RAM to allow the APP to place
 * a .noclear area at the start of RAM.
 * This way the .noclear area is undistrubed by the boot loader running.
 *
 * To make things easy for SBL builds and its test APPs just reserve
 * the last 16kB for BL, leaving the rest for test APPs.
 */

MEMORY
{
  BL_CODE  (rx) : ORIGIN = 0x00000000, LENGTH = 0x08000
  BL_STATE (rw) : ORIGIN = 0x00008000, LENGTH = 0x02000
  MFI_DATA (rw) : ORIGIN = 0x0000A000, LENGTH = 0x01000
  SLOT0 (rw)    : ORIGIN = 0x0000B000, LENGTH = 0x78000
  SLOT1 (rw)    : ORIGIN = 0x00083000, LENGTH = 0x79000
  APP_DATA (rw) : ORIGIN = 0x000FC000, LENGTH = 0x04000
  APP_RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x3c000
  BL_RAM (rwx)  : ORIGIN = 0x2003c000, LENGTH = 0x04000
  LOGDATA (rw)  : ORIGIN = LOGDATA_BASE, LENGTH = (LOGDATA_BASE+LOGDATA_LEN)
}

/* Needed to pick up _exit from nano.sys - so need to rescan libraries at end */
/*
 * GROUP(-lgcc -lc -lnosys)
 */
GROUP(-lgcc -lc)

/* Entry Point */
ENTRY(Reset_Handler)

#define ISR_VEC_ALIGN 512
ASSERT(ISR_VEC_ALIGN >= 128, "ISR_VEC_ALIGN must be >= 128")
ASSERT(ISR_VEC_ALIGN == (1 << LOG2CEIL(ISR_VEC_ALIGN)), "ISR_VEC_ALIGN must be power of 2")
ASSERT(ISR_VEC_ALIGN >= isr_end - isr_start, "ISR_VEC_ALIGN must be >= length of ISR vectors")

SECTIONS
{
  .signature :
  {
    KEEP(*(.signature))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    /* Vectors */
    . = ALIGN(ISR_VEC_ALIGN);
    isr_start = .;
    KEEP(*(.isr_vector))
    isr_end = .;

    /* Code */
    . = ALIGN(4);
    *(.text*)
    *(.gnu.linkonce.t.*)

    KEEP(*(SORT(.protect*)))
  } > FLASH

  .rodata :
  {
    . = ALIGN(4);

    KEEP(*(.init))
    KEEP(*(.fini))

    *crtbegin.o(.ctors)
    *crtbegin?.o(.ctors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
    *(SORT(.ctors.*))
    *(.ctors)

    *crtbegin.o(.dtors)
    *crtbegin?.o(.dtors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
    *(SORT(.dtors.*))
    *(.dtors)

    *(.rodata .rodata.*)
    *(.srodata .srodata.*)
    *(.gnu.linkonce.r.*)

    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    . = ALIGN(4);

    /* preinit data */
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);

    . = ALIGN(4);
    /* init data */
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    PROVIDE_HIDDEN (__init_array_end = .);

    . = ALIGN(4);
    /* finit data */
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > FLASH

  . = ALIGN(4);
  __etext = .;

  /* Don't clear this area on startup */
  .noclear (NOLOAD) :
  {
    . = ALIGN(4);
    noclear_start__ = .;
    KEEP(*(SORT_BY_NAME(.noclear.*)))
    noclear_end__ = .;
  } >RAM

  .data : AT (__etext)
  {
    . = ALIGN(4);
    __data_start__ = .;
    _sdata = .;
    *(vtable)
    *(.data .data.*)
    *(.gnu.linkonce.d*)
    *(.sdata*)
    *(.gnu.linkonce.s*)


    . = ALIGN(4);
    _edata = .;
    __data_end__ = .;
  } > RAM

  .bss :
  {
    . = ALIGN(4);
    __bss_start__ = .;
    _sbss = .;
    *(.sbss .sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.bss .bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = .;
  } > RAM

  .heap (COPY):
  {
    . = ALIGN(4);
    __HeapBase = .;
    __end__ = .;
    PROVIDE(end = .);
    KEEP(*(.heap*))
    . = ALIGN(4);
    __HeapLimit = .;
  } > RAM

  /* .stack_dummy section doesn't contains any symbols. It is only
   * used for linker to calculate size of stack sections, and assign
   * values to stack symbols later */
  .stack_dummy (COPY):
  {
    KEEP(*(.stack*))
  } > RAM

  /* Set stack top to end of RAM, and stack limit move down by
   * size of stack_dummy section */
  __StackTop = ORIGIN(RAM) + LENGTH(RAM);
  __StackLimit = __StackTop - SIZEOF(.stack_dummy);
  PROVIDE(__stack = __StackTop);

  /* Check if data + heap + stack exceeds RAM limit */
  ASSERT(__StackLimit >= __HeapLimit, "RAM overflowed")

  /* Check if text sections + data exceeds FLASH limit */
  DataInitFlashUsed = __bss_start__ - __data_start__;
  CodeFlashUsed = __etext - ORIGIN(FLASH);
  TotalFlashUsed = CodeFlashUsed + DataInitFlashUsed;
  ASSERT(TotalFlashUsed <= LENGTH(FLASH), "region FLASH overflowed with .data and user data")

  /* We don't support C++ exceptions, or thread local storage, or java */
  /DISCARD/ : {
    *(.note .note.*)
    *(.tdata .tdata.* .gnu.linkonce.td.*)
    *(.tbss .tbss.* .gnu.linkonce.tb.*)
    *(.tcommon)
    *(.eh_frame .eh_frame.*)
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    *(.ARM.exidx*)
    *(.jcr*)
  }
}
