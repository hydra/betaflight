/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#include "build/build_config.h"

#include "drivers/system.h"
#include "drivers/flash.h"

#include "configio.h"

#if defined(CONFIG_IN_FLASH)

#if defined(STM32H750xx)
#error "STM32750xx only has one flash page which contains the bootloader, no spare flash pages available, use external storage for persistent config or ram for target testing"
#endif

configIO_t configIOFlash;

// @todo this is not strictly correct for F4/F7, where sector sizes are variable
#if !defined(FLASH_PAGE_SIZE)
// F1
# if defined(STM32F10X_MD)
#  define FLASH_PAGE_SIZE                 (0x400)
# elif defined(STM32F10X_HD)
#  define FLASH_PAGE_SIZE                 (0x800)
// F3
# elif defined(STM32F303xC)
#  define FLASH_PAGE_SIZE                 (0x800)
// F4
# elif defined(STM32F40_41xxx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x4000) // 16K sectors
# elif defined (STM32F411xE)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x4000)
# elif defined(STM32F427_437xx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x4000)
# elif defined (STM32F446xx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x4000)
// F7
#elif defined(STM32F722xx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x4000) // 16K sectors
# elif defined(STM32F745xx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x8000) // 32K sectors
# elif defined(STM32F746xx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x8000)
# elif defined(STM32F765xx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x8000)
# elif defined(UNIT_TEST)
#  define FLASH_PAGE_SIZE                 (0x400)
// H7
# elif defined(STM32H743xx) || defined(STM32H750xx)
#  define FLASH_PAGE_SIZE                 ((uint32_t)0x20000) // 128K sectors
# else
#  error "Flash page size not defined for target."
# endif
#endif

#if defined(STM32F745xx) || defined(STM32F746xx) || defined(STM32F765xx)
/*
Sector 0    0x08000000 - 0x08007FFF 32 Kbytes
Sector 1    0x08008000 - 0x0800FFFF 32 Kbytes
Sector 2    0x08010000 - 0x08017FFF 32 Kbytes
Sector 3    0x08018000 - 0x0801FFFF 32 Kbytes
Sector 4    0x08020000 - 0x0803FFFF 128 Kbytes
Sector 5    0x08040000 - 0x0807FFFF 256 Kbytes
Sector 6    0x08080000 - 0x080BFFFF 256 Kbytes
Sector 7    0x080C0000 - 0x080FFFFF 256 Kbytes

F7X5XI device with 2M flash
Sector 8    0x08100000 - 0x0813FFFF 256 Kbytes
Sector 9    0x08140000 - 0x0817FFFF 256 Kbytes
Sector 10   0x08180000 - 0x081BFFFF 256 Kbytes
Sector 11   0x081C0000 - 0x081FFFFF 256 Kbytes
*/

static uint32_t getFLASHSectorForEEPROM(void)
{
    if ((uint32_t)&__config_start <= 0x08007FFF)
        return FLASH_SECTOR_0;
    if ((uint32_t)&__config_start <= 0x0800FFFF)
        return FLASH_SECTOR_1;
    if ((uint32_t)&__config_start <= 0x08017FFF)
        return FLASH_SECTOR_2;
    if ((uint32_t)&__config_start <= 0x0801FFFF)
        return FLASH_SECTOR_3;
    if ((uint32_t)&__config_start <= 0x0803FFFF)
        return FLASH_SECTOR_4;
    if ((uint32_t)&__config_start <= 0x0807FFFF)
        return FLASH_SECTOR_5;
    if ((uint32_t)&__config_start <= 0x080BFFFF)
        return FLASH_SECTOR_6;
    if ((uint32_t)&__config_start <= 0x080FFFFF)
        return FLASH_SECTOR_7;
#if defined(STM32F765xx)
    if ((uint32_t)&__config_start <= 0x0813FFFF)
        return FLASH_SECTOR_8;
    if ((uint32_t)&__config_start <= 0x0817FFFF)
        return FLASH_SECTOR_9;
    if ((uint32_t)&__config_start <= 0x081BFFFF)
        return FLASH_SECTOR_10;
    if ((uint32_t)&__config_start <= 0x081FFFFF)
        return FLASH_SECTOR_11;
#endif

    // Not good
    while (1) {
        failureMode(FAILURE_CONFIG_STORE_FAILURE);
    }
}

#elif defined(STM32F722xx)
/*
Sector 0    0x08000000 - 0x08003FFF 16 Kbytes
Sector 1    0x08004000 - 0x08007FFF 16 Kbytes
Sector 2    0x08008000 - 0x0800BFFF 16 Kbytes
Sector 3    0x0800C000 - 0x0800FFFF 16 Kbytes
Sector 4    0x08010000 - 0x0801FFFF 64 Kbytes
Sector 5    0x08020000 - 0x0803FFFF 128 Kbytes
Sector 6    0x08040000 - 0x0805FFFF 128 Kbytes
Sector 7    0x08060000 - 0x0807FFFF 128 Kbytes
*/

static uint32_t getFLASHSectorForEEPROM(void)
{
    if ((uint32_t)&__config_start <= 0x08003FFF)
        return FLASH_SECTOR_0;
    if ((uint32_t)&__config_start <= 0x08007FFF)
        return FLASH_SECTOR_1;
    if ((uint32_t)&__config_start <= 0x0800BFFF)
        return FLASH_SECTOR_2;
    if ((uint32_t)&__config_start <= 0x0800FFFF)
        return FLASH_SECTOR_3;
    if ((uint32_t)&__config_start <= 0x0801FFFF)
        return FLASH_SECTOR_4;
    if ((uint32_t)&__config_start <= 0x0803FFFF)
        return FLASH_SECTOR_5;
    if ((uint32_t)&__config_start <= 0x0805FFFF)
        return FLASH_SECTOR_6;
    if ((uint32_t)&__config_start <= 0x0807FFFF)
        return FLASH_SECTOR_7;

    // Not good
    while (1) {
        failureMode(FAILURE_CONFIG_STORE_FAILURE);
    }
}

#elif defined(STM32F4)
/*
Sector 0    0x08000000 - 0x08003FFF 16 Kbytes
Sector 1    0x08004000 - 0x08007FFF 16 Kbytes
Sector 2    0x08008000 - 0x0800BFFF 16 Kbytes
Sector 3    0x0800C000 - 0x0800FFFF 16 Kbytes
Sector 4    0x08010000 - 0x0801FFFF 64 Kbytes
Sector 5    0x08020000 - 0x0803FFFF 128 Kbytes
Sector 6    0x08040000 - 0x0805FFFF 128 Kbytes
Sector 7    0x08060000 - 0x0807FFFF 128 Kbytes
Sector 8    0x08080000 - 0x0809FFFF 128 Kbytes
Sector 9    0x080A0000 - 0x080BFFFF 128 Kbytes
Sector 10   0x080C0000 - 0x080DFFFF 128 Kbytes
Sector 11   0x080E0000 - 0x080FFFFF 128 Kbytes
*/

static uint32_t getFLASHSectorForEEPROM(void)
{
    if ((uint32_t)&__config_start <= 0x08003FFF)
        return FLASH_Sector_0;
    if ((uint32_t)&__config_start <= 0x08007FFF)
        return FLASH_Sector_1;
    if ((uint32_t)&__config_start <= 0x0800BFFF)
        return FLASH_Sector_2;
    if ((uint32_t)&__config_start <= 0x0800FFFF)
        return FLASH_Sector_3;
    if ((uint32_t)&__config_start <= 0x0801FFFF)
        return FLASH_Sector_4;
    if ((uint32_t)&__config_start <= 0x0803FFFF)
        return FLASH_Sector_5;
    if ((uint32_t)&__config_start <= 0x0805FFFF)
        return FLASH_Sector_6;
    if ((uint32_t)&__config_start <= 0x0807FFFF)
        return FLASH_Sector_7;
    if ((uint32_t)&__config_start <= 0x0809FFFF)
        return FLASH_Sector_8;
    if ((uint32_t)&__config_start <= 0x080DFFFF)
        return FLASH_Sector_9;
    if ((uint32_t)&__config_start <= 0x080BFFFF)
        return FLASH_Sector_10;
    if ((uint32_t)&__config_start <= 0x080FFFFF)
        return FLASH_Sector_11;

    // Not good
    while (1) {
        failureMode(FAILURE_CONFIG_STORE_FAILURE);
    }
}

#elif defined(STM32H743xx)
/*
There are two banks of 8 of 128K sectors (up to 2MB flash)

Bank 1
Sector 0    0x08000000 - 0x0801FFFF 128 Kbytes
Sector 1    0x08020000 - 0x0803FFFF 128 Kbytes
Sector 2    0x08040000 - 0x0805FFFF 128 Kbytes
Sector 3    0x08060000 - 0x0807FFFF 128 Kbytes
Sector 4    0x08080000 - 0x0809FFFF 128 Kbytes
Sector 5    0x080A0000 - 0x080BFFFF 128 Kbytes
Sector 6    0x080C0000 - 0x080DFFFF 128 Kbytes
Sector 7    0x080E0000 - 0x080FFFFF 128 Kbytes

Bank 2
Sector 0    0x08100000 - 0x0811FFFF 128 Kbytes
Sector 1    0x08120000 - 0x0813FFFF 128 Kbytes
Sector 2    0x08140000 - 0x0815FFFF 128 Kbytes
Sector 3    0x08160000 - 0x0817FFFF 128 Kbytes
Sector 4    0x08180000 - 0x0819FFFF 128 Kbytes
Sector 5    0x081A0000 - 0x081BFFFF 128 Kbytes
Sector 6    0x081C0000 - 0x081DFFFF 128 Kbytes
Sector 7    0x081E0000 - 0x081FFFFF 128 Kbytes

*/

static void getFLASHSectorForEEPROM(uint32_t *bank, uint32_t *sector)
{
    uint32_t start = (uint32_t)&__config_start;

    if (start >= FLASH_BANK1_BASE && start < FLASH_BANK2_BASE) {
        *bank = FLASH_BANK_1;
    } else if (start >= FLASH_BANK2_BASE && start < FLASH_BANK2_BASE + 0x100000) {
        *bank = FLASH_BANK_2;
        start -= 0x100000;
    } else {
        // Not good
        while (1) {
            failureMode(FAILURE_CONFIG_STORE_FAILURE);
        }
    }

    if (start <= 0x0801FFFF)
        *sector = FLASH_SECTOR_0;
    else if (start <= 0x0803FFFF)
        *sector = FLASH_SECTOR_1;
    else if (start <= 0x0805FFFF)
        *sector = FLASH_SECTOR_2;
    else if (start <= 0x0807FFFF)
        *sector = FLASH_SECTOR_3;
    else if (start <= 0x0809FFFF)
        *sector = FLASH_SECTOR_4;
    else if (start <= 0x080BFFFF)
        *sector = FLASH_SECTOR_5;
    else if (start <= 0x080DFFFF)
        *sector = FLASH_SECTOR_6;
    else if (start <= 0x080FFFFF)
        *sector = FLASH_SECTOR_7;
}
#elif defined(STM32H750xx)
/*
The memory map supports 2 banks of 8 128k sectors like the H743xx, but there is only one 128K sector so we save some code
space by using a smaller function.

Bank 1
Sector 0    0x08000000 - 0x0801FFFF 128 Kbytes

*/

static void getFLASHSectorForEEPROM(uint32_t *bank, uint32_t *sector)
{

    uint32_t start = (uint32_t)&__config_start;

    if (start == FLASH_BANK1_BASE) {
        *sector = FLASH_SECTOR_0;
        *bank = FLASH_BANK_1;
    } else {
        // Not good
        while (1) {
            failureMode(FAILURE_CONFIG_STORE_FAILURE);
        }
    }
}
#endif

#if defined(STM32H7)
#define CONFIG_STREAMER_BUFFER_SIZE 32  // Flash word = 256-bits
typedef uint64_t config_streamer_buffer_align_type_t;
#else
#define CONFIG_STREAMER_BUFFER_SIZE 4
typedef uint32_t config_streamer_buffer_align_type_t;
#endif

typedef union {
    uint8_t b[CONFIG_STREAMER_BUFFER_SIZE];
    config_streamer_buffer_align_type_t w;
} alignedBuffer_t;

alignedBuffer_t alignedBuffer;
bool unlocked;


void configIOFlashInit(void)
{
	// Nothing to do, config is read from flash by using __config_start as defined by linker scripts.
}

bool configIOFlashRead(void)
{
	return true;
}

void configIOFlashBeginWrite(configIO_t * const io, uintptr_t base, int size)
{
    memset(&alignedBuffer, 0, sizeof(alignedBuffer));
	unlocked = false;

    io->err = 0;
	io->at = 0;

	io->address = base;
	io->size = size;

    if (!unlocked) {
#if defined(STM32F7) || defined(STM32H7)
        HAL_FLASH_Unlock();
#else
        FLASH_Unlock();
#endif
        unlocked = true;
    }

#if defined(STM32F10X)
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
#elif defined(STM32F303)
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
#elif defined(STM32F4)
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
#elif defined(STM32F7)
	// NOP
#elif defined(STM32H7)
	// NOP
#else
# error "Unsupported CPU"
#endif
}

// FIXME the return values are currently magic numbers
static int write_word(struct configIO_s * const io, config_streamer_buffer_align_type_t *buffer)
{
    if (io->err != 0) {
        return io->err;
    }

#if defined(STM32H7)
    if (io->address % FLASH_PAGE_SIZE == 0) {
        FLASH_EraseInitTypeDef EraseInitStruct = {
            .TypeErase     = FLASH_TYPEERASE_SECTORS,
            .VoltageRange  = FLASH_VOLTAGE_RANGE_3, // 2.7-3.6V
            .NbSectors     = 1
        };
        getFLASHSectorForEEPROM(&EraseInitStruct.Banks, &EraseInitStruct.Sector);
        uint32_t SECTORError;
        const HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError);
        if (status != HAL_OK) {
            return -1;
        }
    }

    // For H7
    // HAL_StatusTypeDef HAL_FLASH_Program(uint32_t TypeProgram, uint32_t Address, uint64_t DataAddress);
    const HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, io->address, (uint64_t)(uint32_t)buffer);
    if (status != HAL_OK) {
        return -2;
    }
#elif defined(STM32F7)
    if (io->address % FLASH_PAGE_SIZE == 0) {
        FLASH_EraseInitTypeDef EraseInitStruct = {
            .TypeErase     = FLASH_TYPEERASE_SECTORS,
            .VoltageRange  = FLASH_VOLTAGE_RANGE_3, // 2.7-3.6V
            .NbSectors     = 1
        };
        EraseInitStruct.Sector = getFLASHSectorForEEPROM();
        uint32_t SECTORError;
        const HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError);
        if (status != HAL_OK) {
            return -1;
        }
    }

    // For F7
    // HAL_StatusTypeDef HAL_FLASH_Program(uint32_t TypeProgram, uint32_t Address, uint64_t Data);
    const HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, io->address, (uint64_t)*buffer);
    if (status != HAL_OK) {
        return -2;
    }
#else // !STM32H7 && !STM32F7
    if (io->address % FLASH_PAGE_SIZE == 0) {
#if defined(STM32F4)
        const FLASH_Status status = FLASH_EraseSector(getFLASHSectorForEEPROM(), VoltageRange_3); //0x08080000 to 0x080A0000
#else // STM32F3, STM32F1
        const FLASH_Status status = FLASH_ErasePage(io->address);
#endif
        if (status != FLASH_COMPLETE) {
            return -1;
        }
    }
    const FLASH_Status status = FLASH_ProgramWord(io->address, *buffer);
    if (status != FLASH_COMPLETE) {
        return -2;
    }
#endif
    io->address += CONFIG_STREAMER_BUFFER_SIZE;
    return 0;
}


int configIOFlashAppendByte(configIO_t * const io, const uint8_t byte)
{
	alignedBuffer.b[io->at++] = byte;

	if (io->at == sizeof(alignedBuffer)) {
        io->err = write_word(io, &alignedBuffer.w);
		io->at = 0;
	}

	return io->err;
}

int configIOFlashFlush(configIO_t * const io)
{
    if (io->at != 0) {
        memset(alignedBuffer.b + io->at, 0, sizeof(alignedBuffer) - io->at);
		io->err = write_word(io, &alignedBuffer.w);
        io->at = 0;
    }

    return io->err;
}

int configIOFlashStatus(configIO_t * const io)
{
	return io->err;
}

int configIOFlashFinishWrite(configIO_t * const io)
{
    if (unlocked) {
#if defined(STM32F7) || defined(STM32H7)
        HAL_FLASH_Lock();
#else
        FLASH_Lock();
#endif
        unlocked = false;
    }
    return io->err;
}

size_t configIOFlashGetStorageSize(void)
{
    return &__config_end - &__config_start;
}

static const configIOVTable_t configIOFlashVTable = {
    .init = configIOFlashInit,
    .read = configIOFlashRead,
	.beginWrite = configIOFlashBeginWrite,
	.appendByte = configIOFlashAppendByte,
	.flush = configIOFlashFlush,
	.status = configIOFlashStatus,
	.finishWrite = configIOFlashFinishWrite,
	.getStorageSize = configIOFlashGetStorageSize,
};

configIO_t* configIOFlashDetect(void)
{
	configIOFlash.vTable = &configIOFlashVTable;
	return &configIOFlash;
}

#endif
