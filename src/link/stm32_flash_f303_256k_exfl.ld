/*
*****************************************************************************
**
**  File        : stm32_flash.ld
**
**  Abstract    : Linker script for STM32F30x Device with
**                256KByte FLASH and 40KByte RAM
**
*****************************************************************************
*/

/* Specify the memory areas. */
MEMORY
{
    FLASH (rx)        : ORIGIN = 0x08000000, LENGTH = 256K
    FLASH_CONFIG (r)  : ORIGIN = 0x08040000, LENGTH = 0K /* prevent linker warning but disallow linking if accidentally used */

    RAM (xrw)         : ORIGIN = 0x20000000, LENGTH = 40K
    CCM (xrw)         : ORIGIN = 0x10000000, LENGTH = 8K
    MEMORY_B1 (rx)    : ORIGIN = 0x60000000, LENGTH = 0K
}

REGION_ALIAS("STACKRAM", CCM)
REGION_ALIAS("FASTRAM", CCM)

INCLUDE "stm32_flash.ld"
