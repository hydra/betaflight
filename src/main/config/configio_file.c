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

#ifdef CONFIG_IN_FILE

#include "build/build_config.h"

#include "configio.h"

configIO_t configIOFile;

void loadEEPROMFromFile(void) {
    FLASH_Unlock(); // load existing config file into eepromData
}

void configIOFileInit(void)
{
    loadEEPROMFromFile();
}

void configIOFileRead(void)
{
}

static const configIOVTable_t configIOFileVTable = {
    .init = configIOFileInit,
    .read = configIOFileRead,
};

configIO_t* configIOFileDetect(void)
{
	configIOFile.vTable = &configIOFileVTable;
	return &configIOFile;
}

#endif
