/**
 * @file    motor_bldc.c
 * @brief   Sterownik 2× ESC (RC PWM 50 Hz) na TIM1: mapowanie % → µs, aliasy wyjść.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Upewnij się, że TIM1 ma PSC/ARR dla 50 Hz (1 µs/tick). Kanały: CH1=PA8 (Right), CH4=PA11 (Left).
 *
 * Funkcje w pliku (skrót):
 *   - clamp_i8(int v, int lo, int hi)
 *   - esc_set_ccr(ESC_Channel_t ch, uint16_t us)
 *   - ESC_Init(TIM_HandleTypeDef *htim)
 *   - ESC_ArmNeutral(uint32_t neutral_ms)
 *   - ESC_WritePulseUs(ESC_Channel_t ch, uint16_t us)
 *   - ESC_WritePercentRaw(ESC_Channel_t ch, int8_t percent)
 *   - ESC_SetNeutralAll(void)
 *   - ESC_GetMinUs(void)
 *   - ESC_GetNeuUs(void)
 *   - ESC_GetMaxUs(void)
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
            // Ustaw CCR (µs) dla danego kanału PWM TIM1
__HAL_TIM_SET_COMPARE(s_tim1, TIM_CHANNEL_1, us);
            break;
        case ESC_CH4:
            // Ustaw CCR (µs) dla danego kanału PWM TIM1
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
    // Start PWM na wskazanym kanale (włącza generację sygnału)
HAL_TIM_PWM_Start(s_tim1, TIM_CHANNEL_1);
    // Start PWM na wskazanym kanale (włącza generację sygnału)
HAL_TIM_PWM_Start(s_tim1, TIM_CHANNEL_4);

    /* Bezpieczny stan: neutral na obu kanałach */
    // Wyślij 1500 µs na oba kanały — neutral (bezpieczny stan)
ESC_SetNeutralAll();
}

void ESC_ArmNeutral(uint32_t neutral_ms)
{
    /* Jednorazowe ARM na starcie – celowo blokujące */
    // Wyślij 1500 µs na oba kanały — neutral (bezpieczny stan)
ESC_SetNeutralAll();
    // Odczekaj (ms) — ARM sekwencja dla ESC
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
