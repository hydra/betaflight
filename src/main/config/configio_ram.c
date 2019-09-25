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

#if defined(CONFIG_IN_RAM)

#if defined(PERSISTENT)
PERSISTENT uint8_t eepromData[EEPROM_SIZE];
#else
uint8_t eepromData[EEPROM_SIZE];
#endif

configIO_t configIORam;

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


void configIORamInit(void)
{
}

bool configIORamRead(void)
{
	return true;
}

void configIORamBeginWrite(configIO_t * const io, uintptr_t base, int size)
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


int configIORamAppendByte(configIO_t * const io, const uint8_t byte)
{
	alignedBuffer.b[io->at++] = byte;

	if (io->at == sizeof(alignedBuffer)) {
        io->err = write_word(io, &alignedBuffer.w);
		io->at = 0;
	}

	return io->err;
}

int configIORamFlush(configIO_t * const io)
{
    if (io->at != 0) {
        memset(alignedBuffer.b + io->at, 0, sizeof(alignedBuffer) - io->at);
		io->err = write_word(io, &alignedBuffer.w);
        io->at = 0;
    }

    return io->err;
}

int configIORamStatus(configIO_t * const io)
{
	return io->err;
}

int configIORamFinishWrite(configIO_t * const io)
{
    return io->err;
}

size_t configIORamGetStorageSize(void)
{
    return EEPROM_SIZE;
}

static const configIOVTable_t configIORamVTable = {
    .init = configIORamInit,
    .read = configIORamRead,
	.beginWrite = configIORamBeginWrite,
	.appendByte = configIORamAppendByte,
	.flush = configIORamFlush,
	.status = configIORamStatus,
	.finishWrite = configIORamFinishWrite,
	.getStorageSize = configIORamGetStorageSize,
};

configIO_t* configIORamDetect(void)
{
	configIORam.vTable = &configIORamVTable;
	return &configIORam;
}

#endif
