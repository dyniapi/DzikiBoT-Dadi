/**
 ******************************************************************************
 * @file    motor_bldc.h
 * @brief   Minimalny sterownik ESC (BLDC) – RC PWM 50 Hz na TIM1 (STM32L432KC)
 * @date    2025-10-28
 *
 * Sprzęt/piny:
 *   - TIM1_CH1 → PA8  (Right ESC)
 *   - TIM1_CH4 → PA11 (Left  ESC)
 *
 * Konfiguracja TIM1 w CubeMX (zalecenie):
 *   - Timer clock: 80 MHz (typowo dla L4)
 *   - PSC = 79  → 80 MHz / (79+1) = 1 MHz (1 tick = 1 µs)
 *   - ARR = 19999 → okres = 20000 µs = 20 ms (50 Hz)
 *   - PWM Mode 1, Polarity = HIGH
 *
 * API jest świadomie proste i liniowe:
 *   - ESC_WritePercentRaw(ch, -100..+100) → 1000..2000 µs
 *   - mapowanie/kompensacja/EMA – robi warstwa tank_drive (nie tutaj)
 ******************************************************************************
 */

#ifndef MOTOR_BLDC_H
#define MOTOR_BLDC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "tim.h"
#include <stdint.h>

/* Kanały ESC na TIM1 */
typedef enum {
    ESC_CH1 = 0,   /* TIM1_CH1 → PA8  (Right) */
    ESC_CH4        /* TIM1_CH4 → PA11 (Left)  */
} ESC_Channel_t;

/* Inicjalizacja i uzbrojenie (neutral) */
void     ESC_Init(TIM_HandleTypeDef *htim);
void     ESC_ArmNeutral(uint32_t neutral_ms);     /* blokujące – tylko na starcie */

/* Niskopoziomowe wyjście: µs lub „surowe” % (liniowo wokół neutralu) */
void     ESC_WritePulseUs(ESC_Channel_t ch, uint16_t us);     /* 1000..2000 µs */
void     ESC_WritePercentRaw(ESC_Channel_t ch, int8_t percent); /* -100..+100 → 1..2 ms */
void     ESC_SetNeutralAll(void);

/* Stałe mapowania (pomoc w debugach/UI) */
uint16_t ESC_GetMinUs(void);
uint16_t ESC_GetNeuUs(void);
uint16_t ESC_GetMaxUs(void);

#ifdef __cplusplus
}
#endif
#endif /* MOTOR_BLDC_H */
