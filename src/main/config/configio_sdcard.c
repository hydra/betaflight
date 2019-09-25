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

#if defined(CONFIG_IN_SDCARD)

#include "drivers/system.h"

#include "build/build_config.h"
#include "io/asyncfatfs/asyncfatfs.h"

#include "configio.h"

uint8_t eepromData[EEPROM_SIZE];

configIO_t configIOSDCard;

// XXX needed?
#define CONFIG_STREAMER_BUFFER_SIZE 4
typedef uint32_t config_streamer_buffer_align_type_t;

typedef union {
    uint8_t b[CONFIG_STREAMER_BUFFER_SIZE];
    config_streamer_buffer_align_type_t w;
} alignedBuffer_t;

alignedBuffer_t alignedBuffer;

enum {
    FILE_STATE_NONE = 0,
    FILE_STATE_BUSY = 1,
    FILE_STATE_FAILED,
    FILE_STATE_COMPLETE,
};

uint8_t fileState = FILE_STATE_NONE;

const char *defaultSDCardConfigFilename = "CONFIG.BIN";

void saveEEPROMToSDCardCloseContinue(void)
{
    if (fileState != FILE_STATE_FAILED) {
        fileState = FILE_STATE_COMPLETE;
    }
}

void saveEEPROMToSDCardWriteContinue(afatfsFilePtr_t file)
{
    if (!file) {
        fileState = FILE_STATE_FAILED;
        return;
    }

    uint32_t totalBytesWritten = 0;
    uint32_t bytesWritten = 0;
    bool success;

    do {
        bytesWritten = afatfs_fwrite(file, &eepromData[totalBytesWritten], EEPROM_SIZE - totalBytesWritten);
        totalBytesWritten += bytesWritten;
        success = (totalBytesWritten == EEPROM_SIZE);

        afatfs_poll();
    } while (!success && afatfs_getLastError() == AFATFS_ERROR_NONE);

    if (!success) {
        fileState = FILE_STATE_FAILED;
    }

    while (!afatfs_fclose(file, saveEEPROMToSDCardCloseContinue)) {
        afatfs_poll();
    }
}

bool saveEEPROMToSDCard(void)
{
    fileState = FILE_STATE_BUSY;
    bool result = afatfs_fopen(defaultSDCardConfigFilename, "w+", saveEEPROMToSDCardWriteContinue);
    if (!result) {
        return false;
    }

    while (fileState == FILE_STATE_BUSY) {
        afatfs_poll();
    }

    while (!afatfs_flush()) {
        afatfs_poll();
    };

    return (fileState == FILE_STATE_COMPLETE);
}

void loadEEPROMFromSDCardCloseContinue(void)
{
    if (fileState != FILE_STATE_FAILED) {
        fileState = FILE_STATE_COMPLETE;
    }
}

void loadEEPROMFromSDCardReadContinue(afatfsFilePtr_t file)
{
    if (!file) {
        fileState = FILE_STATE_FAILED;
        return;
    }

    fileState = FILE_STATE_BUSY;

    uint32_t totalBytesRead = 0;
    uint32_t bytesRead = 0;
    bool success;

    if (afatfs_feof(file)) {
        // empty file, nothing to load.
        memset(eepromData, 0x00, EEPROM_SIZE);
        success = true;
    } else {

        do {
            bytesRead = afatfs_fread(file, &eepromData[totalBytesRead], EEPROM_SIZE - totalBytesRead);
            totalBytesRead += bytesRead;
            success = (totalBytesRead == EEPROM_SIZE);

            afatfs_poll();
        } while (!success && afatfs_getLastError() == AFATFS_ERROR_NONE);
    }

    if (!success) {
        fileState = FILE_STATE_FAILED;
    }

    while (!afatfs_fclose(file, loadEEPROMFromSDCardCloseContinue)) {
        afatfs_poll();
    }

    return;
}

bool loadEEPROMFromSDCard(void)
{
    fileState = FILE_STATE_BUSY;
    // use "w+" mode here to ensure the file is created now - in w+ mode we can read and write and the seek position is 0 on existing files, ready for reading.
    bool result = afatfs_fopen(defaultSDCardConfigFilename, "w+", loadEEPROMFromSDCardReadContinue);
    if (!result) {
        return false;
    }

    while (fileState == FILE_STATE_BUSY) {
        afatfs_poll();
    }

    return (fileState == FILE_STATE_COMPLETE);
}

void configIOSDCardInit(void)
{
    bool eepromLoaded = loadEEPROMFromSDCard();
    if (!eepromLoaded) {
        // SDCard read failed - just die now
        failureMode(FAILURE_SDCARD_READ_FAILED);
    }
}

bool configIOSDCardRead(void)
{
	// copy it back from file to the in-memory buffer.
	return loadEEPROMFromSDCard();
}

void configIOSDCardBeginWrite(configIO_t * const io, uintptr_t base, int size)
{
	io->err = 0;
	io->at = 0;

	io->address = base;
	io->size = size;
}

// FIXME the return values are currently magic numbers
static int write_word(configIO_t * const io, config_streamer_buffer_align_type_t *buffer)
{
    if (io->err != 0) {
        return io->err;
    }
    if (io->address == (uintptr_t)&eepromData[0]) {
        memset(eepromData, 0, sizeof(eepromData));
    }

    uint64_t *dest_addr = (uint64_t *)io->address;
    uint64_t *src_addr = (uint64_t*)buffer;
    uint8_t row_index = 4;
    /* copy the 256 bits flash word */
    do
    {
      *dest_addr++ = *src_addr++;
    } while (--row_index != 0);
    io->address += CONFIG_STREAMER_BUFFER_SIZE;
    return 0;
}


int configIOSDCardAppendByte(configIO_t * const io, const uint8_t byte)
{
	alignedBuffer.b[io->at++] = byte;

	if (io->at == sizeof(alignedBuffer)) {
        io->err = write_word(io, &alignedBuffer.w);
		io->at = 0;
	}

	return io->err;
}


int configIOSDCardFlush(configIO_t * const io)
{
    if (io->at != 0) {
        memset(alignedBuffer.b + io->at, 0, sizeof(alignedBuffer) - io->at);
		io->err = write_word(io, &alignedBuffer.w);
        io->at = 0;
    }

    return io->err;
}

int configIOSDCardFinishWrite(configIO_t * const io)
{
	UNUSED(io);
	return saveEEPROMToSDCard();
}

int configIOSDCardStatus(configIO_t * const io)
{
	return io->err;
}

size_t configIOSDCardGetStorageSize(void)
{
    return &__config_end - &__config_start;
}

static const configIOVTable_t configIOSDCardVTable = {
    .init = configIOSDCardInit,
    .read = configIOSDCardRead,
	.appendByte = configIOSDCardAppendByte,
	.flush = configIOSDCardFlush,
	.status = configIOSDCardStatus,
	.beginWrite = configIOSDCardBeginWrite,
	.finishWrite = configIOSDCardFinishWrite,
	.getStorageSize = configIOSDCardGetStorageSize,
};

configIO_t* configIOSDCardDetect(void)
{
	configIOSDCard.vTable = &configIOSDCardVTable;
	return &configIOSDCard;
}

#endif
