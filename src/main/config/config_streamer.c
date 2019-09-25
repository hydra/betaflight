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

#include <string.h>

#include "platform.h"

#include "drivers/system.h"
#include "drivers/flash.h"

#include "config/config_streamer.h"
#include "config/configio.h"

void config_streamer_init(config_streamer_t *c, configIO_t *io)
{
    c->io = io;
}

void config_streamer_start(config_streamer_t *c, uintptr_t base, int size)
{
    // base must start at FLASH_PAGE_SIZE boundary when using embedded flash.
	c->io->vTable->beginWrite(c->io, base, size);
}


int config_streamer_write(config_streamer_t *c, const uint8_t *p, uint32_t size)
{
	int err = c->io->vTable->status(c->io);
    for (const uint8_t *pat = p; pat != (uint8_t*)p + size; pat++) {
		err = c->io->vTable->appendByte(c->io, *pat);
    }
    return err;
}

int config_streamer_status(config_streamer_t *c)
{
    return c->io->vTable->status(c->io);
}

int config_streamer_flush(config_streamer_t *c)
{
	return c->io->vTable->flush(c->io);
}

int config_streamer_finish(config_streamer_t *c)
{
    return c->io->vTable->finishWrite(c->io);
}
