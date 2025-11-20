#pragma once
/*
 * ============================================================================
 *  MODULE: motor_bldc — wyjście 2×ESC (RC PWM 50 Hz) na TIM1
 *  ----------------------------------------------------------------------------
 *  CO:
 *    - Prosta warstwa „elektryczna” — mapuje % na mikrosekundy i zapisuje do CCR.
 *    - Dwa kanały: TIM1_CH1 (PA8) = Right, TIM1_CH4 (PA11) = Left.
 *
 *  PO CO:
 *    - Oddzielenie pracy z timerem/CCR od logiki napędu (tank_drive).
 *    - Jedno miejsce, gdzie w razie potrzeby korygujesz zakres µs (np. 1000/1500/2000).
 *
 *  KIEDY:
 *    - ESC_Init(&htim1)          — po starcie PWM na TIM1 (CubeMX).
 *    - ESC_ArmNeutral(ms)        — jednorazowo po starcie (neutral 1500 µs przez ms).
 *    - ESC_WritePercentRaw(ch,%) — w każdej iteracji Tank_Update (już po mapowaniu do okna ESC).
 *    - ESC_SetNeutralAll()       — natychmiastowy neutral na obu kanałach (stan bezpieczny).
 *
 *  SKALA:
 *    - ESC_WritePercentRaw: „surowy” % w zakresie −100..0..+100 (0 = 1500 µs; ±100 = skraje).
 *      Przycięcie do zakresu odbywa się w implementacji.
 * ============================================================================
 *  Typowe zakresy strojenia okna RC (µs)
 *
 *  ESC_MIN_US :  980..1100 µs   (typ. 1000)   „pełny wstecz”
 *  ESC_NEU_US : 1500 µs         (stałe)       neutral
 *  ESC_MAX_US : 1900..2020 µs   (typ. 2000)   „pełny naprzód”
 *
 *  Wskazówki:
 *    • Jeśli ESC kalibrowane ręcznie — ustaw tu wartości końcowe z kalibracji.
 *    • Gdy napęd „szarpie” przy starcie: zostaw MIN/NEU/MAX, a zwiększ esc_start_pct w CFG_Motors().
 *    • Upewnij się, że TIM1 daje ~50 Hz (ok. 20 ms okresu) i skala CCR odpowiada 1 µs.
 * -------------------------------------------------------------------------- */


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>                 // int8_t, uint16_t, uint32_t
#include "stm32l4xx_hal.h"          // TIM_HandleTypeDef (uchwyt timera z CubeMX)

/* ----------------------------------------------------------------------------
 *  Kanały ESC na TIM1 — przyporządkowanie fizycznych wyjść (spójne w całym projekcie).
 *  ESC_CH1 → TIM1_CH1 = PA8  (Right)
 *  ESC_CH4 → TIM1_CH4 = PA11 (Left)
 * ---------------------------------------------------------------------------- */
typedef enum {
    ESC_CH1 = 0,                     // Right  — TIM1 Channel 1 (PA8)
    ESC_CH4 = 1,                     // Left   — TIM1 Channel 4 (PA11)
} ESC_Channel_t;

/* ----------------------------------------------------------------------------
 *  Inicjalizacja warstwy ESC: zapamiętuje uchwyt TIM1, startuje PWM na CH1 i CH4,
 *  oraz ustawia neutral na obu kanałach. Wywołaj raz po starcie systemu.
 * ---------------------------------------------------------------------------- */
void ESC_Init(TIM_HandleTypeDef *htim);

/* ----------------------------------------------------------------------------
 *  ARM sekwencja: wysyła neutral (1500 µs) na oba kanały i czeka neutral_ms.
 *  UWAGA: Blokujące (HAL_Delay wewnątrz) — używać tylko w App_Init na starcie.
 * ---------------------------------------------------------------------------- */
void ESC_ArmNeutral(uint32_t neutral_ms);

/* ----------------------------------------------------------------------------
 *  Bezpośredni zapis impulsu w mikrosekundach (z przycięciem do okna min..max).
 *  Użyteczne do diagnostyki lub ręcznego sterowania poza mapowaniem „%”.
 * ---------------------------------------------------------------------------- */
void ESC_WritePulseUs(ESC_Channel_t ch, uint16_t us);

/* ----------------------------------------------------------------------------
 *  „Surowy” zapis w skali −100..0..+100, liniowo mapowany na 1000..1500..2000 µs.
 *  Ten poziom zakłada, że logika już uwzględniła okno ESC (esc_start_pct..esc_max_pct).
 * ---------------------------------------------------------------------------- */
void ESC_WritePercentRaw(ESC_Channel_t ch, int8_t percent);

/* ----------------------------------------------------------------------------
 *  Ustaw neutral (1500 µs) na obu kanałach jednocześnie — stan bezpieczny.
 * ---------------------------------------------------------------------------- */
void ESC_SetNeutralAll(void);

/* ----------------------------------------------------------------------------
 *  Gettery zakresów — przydatne do diagnostyki (OLED/UART) i weryfikacji mapowania.
 * ---------------------------------------------------------------------------- */
uint16_t ESC_GetMinUs(void);         // zwraca dolną granicę (np. 1000 µs)
uint16_t ESC_GetNeuUs(void);         // zwraca neutral (np. 1500 µs)
uint16_t ESC_GetMaxUs(void);         // zwraca górną granicę (np. 2000 µs)

#ifdef __cplusplus
}
#endif
