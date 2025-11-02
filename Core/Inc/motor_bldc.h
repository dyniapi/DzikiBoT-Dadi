/**
 * @file    motor_bldc.h
 * @brief   Sterownik 2× ESC (RC PWM 50 Hz) na TIM1: mapowanie % → µs, aliasy wyjść.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Upewnij się, że TIM1 ma PSC/ARR dla 50 Hz (1 µs/tick). Kanały: CH1=PA8 (Right), CH4=PA11 (Left).
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
