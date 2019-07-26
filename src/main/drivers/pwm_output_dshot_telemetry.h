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

#ifdef USE_DSHOT_TELEMETRY

#include <string.h>

extern uint32_t readDoneCount;

// TODO remove once debugging no longer needed
FAST_RAM_ZERO_INIT extern uint32_t dshotInvalidPacketCount;
FAST_RAM_ZERO_INIT extern uint32_t inputBuffer[GCR_TELEMETRY_INPUT_LEN];
FAST_RAM_ZERO_INIT extern uint32_t setDirectionMicros;
FAST_RAM_ZERO_INIT extern uint32_t inputStampUs;

#ifdef USE_DSHOT_TELEMETRY_STATS

void updateDshotTelemetryQuality(uint32_t deviceIndex, bool packetValid, timeMs_t currentTimeMs);
int16_t getDshotTelemetryMotorInvalidPercent(uint8_t motorIndex);
#endif

bool isDshotMotorTelemetryActive(uint8_t motorIndex);

uint32_t decodeTelemetryPacket(uint32_t buffer[], uint32_t count);

#endif
