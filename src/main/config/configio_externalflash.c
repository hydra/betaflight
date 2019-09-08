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

configIO_t configIOExternalFlash;

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

void configIOExternalFlashRead(void)
{
}

static const configIOVTable_t configIOExternalFlashVTable = {
    .init = configIOExternalFlashInit,
    .read = configIOExternalFlashRead,
};

configIO_t* configIOExternalFlashDetect(void)
{
	configIOExternalFlash.vTable = &configIOExternalFlashVTable;
	return &configIOExternalFlash;
}

#endif

