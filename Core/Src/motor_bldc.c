/**
 * @file    motor_bldc.c
 * @brief   Sterownik 2×ESC (RC PWM 50 Hz) na TIM1: mapowanie % → µs + aliasy kanałów.
 * @date    2025-11-02
 *
 * CO:
 *   - Warstwa wyjściowa dla ESC pracujących w standardzie RC PWM (ok. 50 Hz, 1..2 ms).
 *   - Dwa kanały: TIM1_CH1 = PA8 (Right), TIM1_CH4 = PA11 (Left) — zgodnie z projektem.
 *
 * PO CO:
 *   - Oddzielenie „elektryki” (TIM/CCR) od logiki napędu (tank_drive).
 *   - Jedno miejsce, gdzie ewentualnie zmieniasz mapowanie % ↔ mikrosekundy.
 *
 * KIEDY:
 *   - ESC_Init(&htim1)          — po starcie TIM1 i konfiguracji PWM (PSC/ARR = 50 Hz).
 *   - ESC_ArmNeutral(ms)        — jednorazowy ARM po starcie (neutral 1500 µs przez ms).
 *   - ESC_WritePercentRaw(ch,%) — w każdej iteracji napędu (Tank_Update).
 *   - ESC_SetNeutralAll()       — ustaw neutral na obu kanałach (bezpieczny stan).
 *
 * PARAMETRY (stałe w tym pliku):
 *   - ESC_MIN_US / ESC_NEU_US / ESC_MAX_US — granice okna RC (typowo 1000/1500/2000 µs).
 *     Jeśli Twoje ESC wymaga innego zakresu, zmieniasz TYLKO te trzy wartości.
 */

#include "motor_bldc.h"      // typy: ESC_Channel_t, deklaracje API
#include "stm32l4xx_hal.h"   // __HAL_TIM_SET_COMPARE, HAL_TIM_PWM_Start

/* ===================== K O N F I G U R A C J A  O K N A  R C ===================== */
/* Uwaga: To są granice mikrosekund sygnału RC. „Surowy” % (−100..0..+100) mapuje się
 *        w ESC_WritePercentRaw() liniowo na 1000..1500..2000 µs.
 *        Jeśli ESC ma inne wymagania (np. 1100..1900 µs), ZMIEŃ TYLKO te stałe. */
static const uint16_t ESC_MIN_US = 1000;   /* dolna granica: −100% → 1000 µs */
static const uint16_t ESC_NEU_US = 1500;   /* neutral:       0%    → 1500 µs */
static const uint16_t ESC_MAX_US = 2000;   /* górna granica: +100% → 2000 µs */

/* Uchwyt do TIM1 przekazujemy z zewnątrz w ESC_Init(&htim1) */
static TIM_HandleTypeDef *s_tim1 = NULL;

/* Pomocniczy clamp: ogranicz int do [lo..hi], zwróć jako int8_t */
static inline int8_t clamp_i8(int v, int lo, int hi)
{
    if (v < lo) v = lo;                 /* podnieś do minimum, jeśli za mało */
    if (v > hi) v = hi;                 /* zetnij do maksimum, jeśli za dużo */
    return (int8_t)v;                   /* zwróć w zakresie int8_t           */
}

/* esc_set_ccr:
 *  - ustawia CCR na odpowiednim kanale TIM1,
 *  - pilnuje, by mikrosekundy nie wyszły poza [ESC_MIN_US..ESC_MAX_US],
 *  - wywoływane przez wszystkie funkcje zapisujące do PWM. */
static void esc_set_ccr(ESC_Channel_t ch, uint16_t us)
{
    if (!s_tim1) return;                /* brak zainicjalizowanego TIM1 → nic nie rób */

    if (us < ESC_MIN_US) us = ESC_MIN_US;   /* przytnij do dozwolonego minimum  */
    if (us > ESC_MAX_US) us = ESC_MAX_US;   /* przytnij do dozwolonego maksimum */

    switch (ch) {
        case ESC_CH1: /* Right → TIM1_CH1 (PA8) */
            __HAL_TIM_SET_COMPARE(s_tim1, TIM_CHANNEL_1, us); /* wyślij µs do CCR1 */
            break;
        case ESC_CH4: /* Left  → TIM1_CH4 (PA11) */
            __HAL_TIM_SET_COMPARE(s_tim1, TIM_CHANNEL_4, us); /* wyślij µs do CCR4 */
            break;
        default:
            /* nieobsługiwany kanał — nic nie rób (ochrona przed błędnym wywołaniem) */
            break;
    }
}

/* ESC_Init:
 *  - zapamiętuje uchwyt TIM1 i startuje PWM na CH1 oraz CH4,
 *  - natychmiast ustawia neutral na obu kanałach (bezpieczny stan). */
void ESC_Init(TIM_HandleTypeDef *htim)
{
    s_tim1 = htim;                                      /* zachowaj uchwyt do TIM1     */

    HAL_TIM_PWM_Start(s_tim1, TIM_CHANNEL_1);           /* start PWM na CH1 (PA8)      */
    HAL_TIM_PWM_Start(s_tim1, TIM_CHANNEL_4);           /* start PWM na CH4 (PA11)     */

    ESC_SetNeutralAll();                                /* na starcie zawsze neutral   */
}

/* ESC_ArmNeutral:
 *  - wysyła neutral (1500 µs) na oba kanały,
 *  - czeka neutral_ms (blokująco) — wymagane przez wiele ESC do „uzbrojenia”.
 *  - UWAGA: Wołać TYLKO raz na starcie (np. w App_Init). */
void ESC_ArmNeutral(uint32_t neutral_ms)
{
    ESC_SetNeutralAll();                                /* wymuś neutral na CH1 i CH4  */
    HAL_Delay(neutral_ms);                              /* odczekaj ms → ARM sekwencja */
}

/* ESC_WritePulseUs:
 *  - bezpośredni zapis mikrosekund do kanału (z przycięciem do okna). */
void ESC_WritePulseUs(ESC_Channel_t ch, uint16_t us)
{
    esc_set_ccr(ch, us);                                /* ustaw CCR odpowiedniego CH  */
}

/* ESC_WritePercentRaw:
 *  - mapuje „surowy” % w skali −100..0..+100 do 1000..1500..2000 µs (liniowo),
 *  - % jest przeznaczony dla warstwy ESC i już uwzględnia:
 *      • znak (kierunek), okno [esc_start_pct..esc_max_pct] i neutral,
 *      • równoodległe skalowanie (−100% = 1000 µs, 0% = 1500 µs, +100% = 2000 µs).
 *  - używane przez Tank_Update() po mapowaniu z logiki. */
void ESC_WritePercentRaw(ESC_Channel_t ch, int8_t percent)
{
    percent = clamp_i8(percent, -100, 100);             /* zabezpieczenie wejścia      */

    /* Połówkowy rozstaw w µs: (2000−1000)/2 = 500 µs */
    const int32_t span_half = (int32_t)(ESC_MAX_US - ESC_MIN_US) / 2;

    /* Formuła: 1500 µs + (percent/100)*500 µs  */
    int32_t out = (int32_t)ESC_NEU_US + (span_half * (int32_t)percent) / 100;

    esc_set_ccr(ch, (uint16_t)out);                     /* wyślij µs do wskazanego CH  */
}

/* ESC_SetNeutralAll:
 *  - ustawia neutral (1500 µs) jednocześnie na obu kanałach (CH1 i CH4). */
void ESC_SetNeutralAll(void)
{
    esc_set_ccr(ESC_CH1, ESC_NEU_US);                   /* Right → neutral             */
    esc_set_ccr(ESC_CH4, ESC_NEU_US);                   /* Left  → neutral             */
}

/* Gettery: przydatne w diagnostyce / OLED / UART (np. wydruk zakresów). */
uint16_t ESC_GetMinUs(void) { return ESC_MIN_US; }      /* 1000 µs — „pełny wstecz”    */
uint16_t ESC_GetNeuUs(void) { return ESC_NEU_US; }      /* 1500 µs — neutral           */
uint16_t ESC_GetMaxUs(void) { return ESC_MAX_US; }      /* 2000 µs — „pełny naprzód”   */
