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
#include <stdio.h>
#include <string.h>

#include <errno.h>

#include "platform.h"

#ifdef CONFIG_IN_FILE

#include "build/build_config.h"

#include "configio.h"

uint8_t eepromData[EEPROM_SIZE];

configIO_t configIOFile;

#define CONFIG_STREAMER_BUFFER_SIZE 4
typedef uint32_t config_streamer_buffer_align_type_t;

typedef union {
    uint8_t b[CONFIG_STREAMER_BUFFER_SIZE];
    config_streamer_buffer_align_type_t w;
} alignedBuffer_t;

alignedBuffer_t alignedBuffer;

// SIMULATOR
#if !defined(FLASH_PAGE_SIZE)
# if defined(SIMULATOR_BUILD)
#  define FLASH_PAGE_SIZE                 (0x400)
# else
#  error "Flash page size not defined for target."
# endif
#endif


// fake EEPROM
static FILE *eepromFd = NULL;

static void FLASH_Unlock(void) {
    if (eepromFd != NULL) {
        fprintf(stderr, "[FLASH_Unlock] eepromFd != NULL\n");
        return;
    }

    // open or create
    eepromFd = fopen(EEPROM_FILENAME,"r+");
    if (eepromFd != NULL) {
        // obtain file size:
        fseek(eepromFd , 0 , SEEK_END);
        size_t lSize = ftell(eepromFd);
        rewind(eepromFd);

        size_t n = fread(eepromData, 1, sizeof(eepromData), eepromFd);
        if (n == lSize) {
            printf("[FLASH_Unlock] loaded '%s', size = %ld / %ld\n", EEPROM_FILENAME, lSize, sizeof(eepromData));
        } else {
            fprintf(stderr, "[FLASH_Unlock] failed to load '%s'\n", EEPROM_FILENAME);
            return;
        }
    } else {
        printf("[FLASH_Unlock] created '%s', size = %ld\n", EEPROM_FILENAME, sizeof(eepromData));
        if ((eepromFd = fopen(EEPROM_FILENAME, "w+")) == NULL) {
            fprintf(stderr, "[FLASH_Unlock] failed to create '%s'\n", EEPROM_FILENAME);
            return;
        }
        if (fwrite(eepromData, sizeof(eepromData), 1, eepromFd) != 1) {
            fprintf(stderr, "[FLASH_Unlock] write failed: %s\n", strerror(errno));
        }
    }
}

static void FLASH_Lock(void) {
    // flush & close
    if (eepromFd != NULL) {
        fseek(eepromFd, 0, SEEK_SET);
        fwrite(eepromData, 1, sizeof(eepromData), eepromFd);
        fclose(eepromFd);
        eepromFd = NULL;
        printf("[FLASH_Lock] saved '%s'\n", EEPROM_FILENAME);
    } else {
        fprintf(stderr, "[FLASH_Lock] eeprom is not unlocked\n");
    }
}

static  FLASH_Status FLASH_ErasePage(uintptr_t Page_Address) {
    UNUSED(Page_Address);
//    printf("[FLASH_ErasePage]%x\n", Page_Address);
    return FLASH_COMPLETE;
}

static FLASH_Status FLASH_ProgramWord(uintptr_t addr, uint32_t value) {
    if ((addr >= (uintptr_t)eepromData) && (addr < (uintptr_t)ARRAYEND(eepromData))) {
        *((uint32_t*)addr) = value;
        printf("[FLASH_ProgramWord]%p = %08x\n", (void*)addr, *((uint32_t*)addr));
    } else {
            printf("[FLASH_ProgramWord]%p out of range!\n", (void*)addr);
    }
    return FLASH_COMPLETE;
}

void loadEEPROMFromFile(void) {
    FLASH_Unlock(); // load existing config file into eepromData
}

void configIOFileInit(void)
{
    loadEEPROMFromFile();
}

bool configIOFileRead(void)
{
	return true;
}

void configIOFileBeginWrite(configIO_t * const io, uintptr_t base, int size)
{
    memset(&alignedBuffer, 0, sizeof(alignedBuffer));

    io->err = 0;
	io->at = 0;

	io->address = base;
	io->size = size;

	FLASH_Unlock();
}

// FIXME the return values are currently magic numbers
static int write_word(struct configIO_s * const io, config_streamer_buffer_align_type_t *buffer)
{
    if (io->err != 0) {
        return io->err;
    }

    if (io->address % FLASH_PAGE_SIZE == 0) {
        const FLASH_Status status = FLASH_ErasePage(io->address);
        if (status != FLASH_COMPLETE) {
            return -1;
        }
    }
    const FLASH_Status status = FLASH_ProgramWord(io->address, *buffer);
    if (status != FLASH_COMPLETE) {
        return -2;
    }
    io->address += CONFIG_STREAMER_BUFFER_SIZE;
    return 0;
}

int configIOFileAppendByte(configIO_t * const io, const uint8_t byte)
{
	alignedBuffer.b[io->at++] = byte;

	if (io->at == sizeof(alignedBuffer)) {
        io->err = write_word(io, &alignedBuffer.w);
		io->at = 0;
	}

	return io->err;
}

int configIOFileFlush(configIO_t * const io)
{
    if (io->at != 0) {
        memset(alignedBuffer.b + io->at, 0, sizeof(alignedBuffer) - io->at);
		io->err = write_word(io, &alignedBuffer.w);
        io->at = 0;
    }

    return io->err;
}

int configIOFileStatus(configIO_t * const io)
{
	return io->err;
}

int configIOFileFinishWrite(configIO_t * const io)
{
	FLASH_Lock();
    return io->err;
}


static const configIOVTable_t configIOFileVTable = {
    .init = configIOFileInit,
    .read = configIOFileRead,
	.beginWrite = configIOFileBeginWrite,
	.appendByte = configIOFileAppendByte,
	.flush = configIOFileFlush,
	.status = configIOFileStatus,
	.finishWrite = configIOFileFinishWrite,

};

configIO_t* configIOFileDetect(void)
{
	configIOFile.vTable = &configIOFileVTable;
	return &configIOFile;
}

#endif
