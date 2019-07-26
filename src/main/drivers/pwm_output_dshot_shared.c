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
#include <math.h>

#include "platform.h"

#ifdef USE_DSHOT

#include "build/debug.h"

#include "drivers/dma.h"
#include "drivers/dma_reqmap.h"
#include "drivers/io.h"
#include "drivers/nvic.h"
#include "drivers/rcc.h"
#include "drivers/time.h"
#include "drivers/timer.h"
#if defined(STM32F4)
#include "stm32f4xx.h"
#elif defined(STM32F3)
#include "stm32f30x.h"
#endif

#include "pwm_output.h"
#include "drivers/dshot.h"
#include "drivers/dshot_dpwm.h"
#include "drivers/dshot_command.h"

#include "pwm_output_dshot_shared.h"
#include "pwm_output_dshot_telemetry.h"


FAST_RAM_ZERO_INIT uint8_t dmaMotorTimerCount = 0;
#ifdef STM32F7
FAST_RAM_ZERO_INIT motorDmaTimer_t dmaMotorTimers[MAX_DMA_TIMERS];
FAST_RAM_ZERO_INIT motorDmaOutput_t dmaMotors[MAX_SUPPORTED_MOTORS];
#else
motorDmaTimer_t dmaMotorTimers[MAX_DMA_TIMERS];
motorDmaOutput_t dmaMotors[MAX_SUPPORTED_MOTORS];
#endif

motorDmaOutput_t *getMotorDmaOutput(uint8_t index)
{
    return &dmaMotors[index];
}

uint8_t getTimerIndex(TIM_TypeDef *timer)
{
    for (int i = 0; i < dmaMotorTimerCount; i++) {
        if (dmaMotorTimers[i].timer == timer) {
            return i;
        }
    }
    dmaMotorTimers[dmaMotorTimerCount++].timer = timer;
    return dmaMotorTimerCount - 1;
}


FAST_CODE void pwmWriteDshotInt(uint8_t index, uint16_t value)
{
    motorDmaOutput_t *const motor = &dmaMotors[index];

    if (!motor->configured) {
        return;
    }

    /*If there is a command ready to go overwrite the value and send that instead*/
    if (dshotCommandIsProcessing()) {
        value = pwmGetDshotCommand(index);
#ifdef USE_DSHOT_TELEMETRY
        // reset telemetry debug statistics every time telemetry is enabled
        if (value == DSHOT_CMD_SIGNAL_LINE_CONTINUOUS_ERPM_TELEMETRY) {
            dshotInvalidPacketCount = 0;
            readDoneCount = 0;
        }
#endif
        if (value) {
            motor->protocolControl.requestTelemetry = true;
        }
    }

    motor->protocolControl.value = value;

    uint16_t packet = prepareDshotPacket(&motor->protocolControl);
    uint8_t bufferSize;

#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        bufferSize = loadDmaBuffer(&motor->timer->dmaBurstBuffer[timerLookupChannelIndex(motor->timerHardware->channel)], 4, packet);
        motor->timer->dmaBurstLength = bufferSize * 4;
    } else
#endif
    {
        bufferSize = loadDmaBuffer(motor->dmaBuffer, 1, packet);
        motor->timer->timerDmaSources |= motor->timerDmaSource;
#ifdef USE_FULL_LL_DRIVER
        xLL_EX_DMA_SetDataLength(motor->dmaRef, bufferSize);
        xLL_EX_DMA_EnableResource(motor->dmaRef);
#else
        xDMA_SetCurrDataCounter(motor->dmaRef, bufferSize);
        xDMA_Cmd(motor->dmaRef, ENABLE);
#endif
    }
}


#ifdef USE_DSHOT_TELEMETRY

void dshotEnableChannels(uint8_t motorCount);

uint16_t getDshotTelemetry(uint8_t index)
{
    return dmaMotors[index].dshotTelemetryValue;
}

#endif

FAST_CODE void pwmDshotSetDirectionOutput(
    motorDmaOutput_t * const motor, bool output
#ifndef USE_DSHOT_TELEMETRY
#ifdef USE_FULL_LL_DRIVER
    , LL_TIM_OC_InitTypeDef* pOcInit, LL_DMA_InitTypeDef* pDmaInit
#else
    , TIM_OCInitTypeDef *pOcInit, DMA_InitTypeDef* pDmaInit
#endif
#endif
);

#ifdef USE_DSHOT_TELEMETRY

FAST_CODE_NOINLINE bool pwmStartDshotMotorUpdate(void)
{
    if (!useDshotTelemetry) {
        return true;
    }
#ifdef USE_DSHOT_TELEMETRY_STATS
    const timeMs_t currentTimeMs = millis();
#endif
    const timeUs_t currentUs = micros();
    for (int i = 0; i < dshotPwmDevice.count; i++) {
        timeDelta_t usSinceInput = cmpTimeUs(currentUs, inputStampUs);
        if (usSinceInput >= 0 && usSinceInput < dmaMotors[i].dshotTelemetryDeadtimeUs) {
            return false;
        }
        if (dmaMotors[i].isInput) {
#ifdef USE_FULL_LL_DRIVER
            uint32_t edges = GCR_TELEMETRY_INPUT_LEN - xLL_EX_DMA_GetDataLength(dmaMotors[i].dmaRef);
#else
            uint32_t edges = GCR_TELEMETRY_INPUT_LEN - xDMA_GetCurrDataCounter(dmaMotors[i].dmaRef);
#endif

#ifdef USE_FULL_LL_DRIVER
            LL_EX_TIM_DisableIT(dmaMotors[i].timerHardware->tim, dmaMotors[i].timerDmaSource);
#else
            TIM_DMACmd(dmaMotors[i].timerHardware->tim, dmaMotors[i].timerDmaSource, DISABLE);
#endif

            uint16_t value = 0xffff;

            if (edges > MIN_GCR_EDGES) {
                readDoneCount++;
                value = decodeTelemetryPacket(dmaMotors[i].dmaBuffer, edges);
                
#ifdef USE_DSHOT_TELEMETRY_STATS
                bool validTelemetryPacket = false;
#endif
                if (value != 0xffff) {
                    dmaMotors[i].dshotTelemetryValue = value;
                    dmaMotors[i].dshotTelemetryActive = true;
                    if (i < 4) {
                        DEBUG_SET(DEBUG_DSHOT_RPM_TELEMETRY, i, value);
                    }
#ifdef USE_DSHOT_TELEMETRY_STATS
                    validTelemetryPacket = true;
#endif
                } else {
                    dshotInvalidPacketCount++;
                    if (i == 0) {
                        memcpy(inputBuffer,dmaMotors[i].dmaBuffer,sizeof(inputBuffer));
                    }
                }
#ifdef USE_DSHOT_TELEMETRY_STATS
                updateDshotTelemetryQuality(i, validTelemetryPacket, currentTimeMs);
#endif
            }
        }
        pwmDshotSetDirectionOutput(&dmaMotors[i], true);
    }
    inputStampUs = 0;
    dshotEnableChannels(dshotPwmDevice.count);
    return true;
}

bool isDshotMotorTelemetryActive(uint8_t motorIndex)
{
    return dmaMotors[motorIndex].dshotTelemetryActive;
}

#endif // USE_DSHOT_TELEMETRY
#endif // USE_DSHOT
