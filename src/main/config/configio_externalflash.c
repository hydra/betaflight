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

#if defined(CONFIG_IN_EXTERNAL_FLASH)

#include "build/build_config.h"

#include "drivers/system.h"
#include "drivers/flash.h"


#include "configio.h"

uint8_t eepromData[EEPROM_SIZE];

configIO_t configIOExternalFlash;

#define CONFIG_STREAMER_BUFFER_SIZE 8 // Must not be greater than the smallest flash page size of all compiled-in flash devices.
typedef uint32_t config_streamer_buffer_align_type_t;

typedef union {
    uint8_t b[CONFIG_STREAMER_BUFFER_SIZE];
    config_streamer_buffer_align_type_t w;
} alignedBuffer_t;

alignedBuffer_t alignedBuffer;

bool loadEEPROMFromExternalFlash(void)
{
    const flashPartition_t *flashPartition = flashPartitionFindByType(FLASH_PARTITION_TYPE_CONFIG);
    const flashGeometry_t *flashGeometry = flashGetGeometry();

    uint32_t flashStartAddress = flashPartition->startSector * flashGeometry->sectorSize;

    uint32_t totalBytesRead = 0;
    int bytesRead = 0;

    bool success = false;

    do {
        bytesRead = flashReadBytes(flashStartAddress + totalBytesRead, &eepromData[totalBytesRead], EEPROM_SIZE - totalBytesRead);
        if (bytesRead > 0) {
            totalBytesRead += bytesRead;
            success = (totalBytesRead == EEPROM_SIZE);
        }
    } while (!success && bytesRead > 0);

    return success;
}

void configIOExternalFlashInit(void)
{
    bool eepromLoaded = loadEEPROMFromExternalFlash();
    if (!eepromLoaded) {
        // Flash read failed - just die now
        failureMode(FAILURE_FLASH_READ_FAILED);
    }
}

bool configIOExternalFlashRead(void)
{
	// copy it back from flash to the in-memory buffer.
	return loadEEPROMFromExternalFlash();
}

void configIOExternalFlashBeginWrite(configIO_t * const io, uintptr_t base, int size)
{
    memset(&alignedBuffer, 0, sizeof(alignedBuffer));

    io->err = 0;
	io->at = 0;

	io->address = base;
	io->size = size;
}

// FIXME the return values are currently magic numbers
static int write_word(struct configIO_s * const io, config_streamer_buffer_align_type_t *buffer)
{
    if (io->err != 0) {
        return io->err;
    }

    uint32_t dataOffset = (uint32_t)(io->address - (uintptr_t)&eepromData[0]);

    const flashPartition_t *flashPartition = flashPartitionFindByType(FLASH_PARTITION_TYPE_CONFIG);
    const flashGeometry_t *flashGeometry = flashGetGeometry();

    uint32_t flashStartAddress = flashPartition->startSector * flashGeometry->sectorSize;
    uint32_t flashOverflowAddress = ((flashPartition->endSector + 1) * flashGeometry->sectorSize); // +1 to sector for inclusive

    uint32_t flashAddress = flashStartAddress + dataOffset;
    if (flashAddress + CONFIG_STREAMER_BUFFER_SIZE > flashOverflowAddress) {
        return -3; // address is past end of partition
    }

    uint32_t flashSectorSize = flashGeometry->sectorSize;
    uint32_t flashPageSize = flashGeometry->pageSize;

    bool onPageBoundary = (flashAddress % flashPageSize == 0);
    if (onPageBoundary) {

        bool firstPage = (flashAddress == flashStartAddress);
        if (!firstPage) {
            flashPageProgramFinish();
        }

        if (flashAddress % flashSectorSize == 0) {
            flashEraseSector(flashAddress);
        }

        flashPageProgramBegin(flashAddress);
    }

    flashPageProgramContinue((uint8_t *)buffer, CONFIG_STREAMER_BUFFER_SIZE);

    io->address += CONFIG_STREAMER_BUFFER_SIZE;
    return 0;
}

int configIOExternalFlashAppendByte(configIO_t * const io, const uint8_t byte)
{
	alignedBuffer.b[io->at++] = byte;

	if (io->at == sizeof(alignedBuffer)) {
        io->err = write_word(io, &alignedBuffer.w);
		io->at = 0;
	}

	return io->err;
}

int configIOExternalFlashFlush(configIO_t * const io)
{
    if (io->at != 0) {
        memset(alignedBuffer.b + io->at, 0, sizeof(alignedBuffer) - io->at);
		io->err = write_word(io, &alignedBuffer.w);
        io->at = 0;
    }

    return io->err;
}

int configIOExternalFlashStatus(configIO_t * const io)
{
	return io->err;
}

int configIOExternalFlashFinishWrite(configIO_t * const io)
{
	UNUSED(io);

	flashFlush();
	return io->err;
}

size_t configIOExternalFlashGetStorageSize(void)
{
	const flashPartition_t *flashPartition = flashPartitionFindByType(FLASH_PARTITION_TYPE_CONFIG);
	return FLASH_PARTITION_SECTOR_COUNT(flashPartition) * flashGetGeometry()->sectorSize;
}

static const configIOVTable_t configIOExternalFlashVTable = {
    .init = configIOExternalFlashInit,
    .read = configIOExternalFlashRead,
	.beginWrite = configIOExternalFlashBeginWrite,
	.appendByte = configIOExternalFlashAppendByte,
	.flush = configIOExternalFlashFlush,
	.status = configIOExternalFlashStatus,
	.finishWrite = configIOExternalFlashFinishWrite,
	.getStorageSize = configIOExternalFlashGetStorageSize,
};

configIO_t* configIOExternalFlashDetect(void)
{
	configIOExternalFlash.vTable = &configIOExternalFlashVTable;
	return &configIOExternalFlash;
}

#endif

