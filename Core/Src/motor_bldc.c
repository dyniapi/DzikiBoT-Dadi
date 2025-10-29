/**
 ******************************************************************************
 * @file    motor_bldc.c
 * @brief   ESC (BLDC) – RC PWM 50 Hz, TIM1; API maksymalnie proste i liniowe
 * @date    2025-10-28
 *
 * Uwaga:
 *  - Zakładamy konfigurację TIM1: 1 tick = 1 µs (PSC=79), ARR=19999 (20 ms)
 *  - Dzięki temu CCR = liczba mikrosekund impulsu.
 *  - HAL_TIM_PWM_Start włącza MOE (Main Output Enable) dla TIM1 (advanced).
 ******************************************************************************
 */

#include "motor_bldc.h"

/* Stałe okna 1..2 ms – jeśli ESC ma inne, zmień w jednym miejscu */
static const uint16_t ESC_MIN_US = 1000;
static const uint16_t ESC_NEU_US = 1500;
static const uint16_t ESC_MAX_US = 2000;

static TIM_HandleTypeDef *s_tim1 = NULL;

static inline int8_t clamp_i8(int v, int lo, int hi)
{
    if (v < lo) {
        v = lo;
    }
    if (v > hi) {
        v = hi;
    }
    return (int8_t)v;
}

/* Zapis CCR na dany kanał (z zawężeniem do dozwolonego zakresu µs) */
static void esc_set_ccr(ESC_Channel_t ch, uint16_t us)
{
    if (!s_tim1) {
        return;
    }
    if (us < ESC_MIN_US) {
        us = ESC_MIN_US;
    }
    if (us > ESC_MAX_US) {
        us = ESC_MAX_US;
    }

    switch (ch) {
        case ESC_CH1:
            __HAL_TIM_SET_COMPARE(s_tim1, TIM_CHANNEL_1, us);
            break;
        case ESC_CH4:
            __HAL_TIM_SET_COMPARE(s_tim1, TIM_CHANNEL_4, us);
            break;
        default:
            /* nieobsługiwany kanał */
            break;
    }
}

void ESC_Init(TIM_HandleTypeDef *htim)
{
    s_tim1 = htim;

    /* Start PWM na CH1 (PA8) i CH4 (PA11). */
    HAL_TIM_PWM_Start(s_tim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(s_tim1, TIM_CHANNEL_4);

    /* Bezpieczny stan: neutral na obu kanałach */
    ESC_SetNeutralAll();
}

void ESC_ArmNeutral(uint32_t neutral_ms)
{
    /* Jednorazowe ARM na starcie – celowo blokujące */
    ESC_SetNeutralAll();
    HAL_Delay(neutral_ms);
}

void ESC_WritePulseUs(ESC_Channel_t ch, uint16_t us)
{
    esc_set_ccr(ch, us);
}

void ESC_WritePercentRaw(ESC_Channel_t ch, int8_t percent)
{
    /* Liniowe mapowanie wokół neutralu:
       -100% → 1000 µs, 0% → 1500 µs, +100% → 2000 µs */
    percent = clamp_i8(percent, -100, 100);
    const int32_t span_half = (int32_t)(ESC_MAX_US - ESC_MIN_US) / 2; /* 500 us */
    int32_t out = (int32_t)ESC_NEU_US + (span_half * (int32_t)percent) / 100;
    esc_set_ccr(ch, (uint16_t)out);
}

void ESC_SetNeutralAll(void)
{
    esc_set_ccr(ESC_CH1, ESC_NEU_US);
    esc_set_ccr(ESC_CH4, ESC_NEU_US);
}

uint16_t ESC_GetMinUs(void) { return ESC_MIN_US; }
uint16_t ESC_GetNeuUs(void) { return ESC_NEU_US; }
uint16_t ESC_GetMaxUs(void) { return ESC_MAX_US; }
