#pragma once
/*
 * ============================================================================
 *  DzikiBoT — globalna konfiguracja runtime (STM32 Nucleo-L432KC)
 *  ----------------------------------------------------------------------------
 *  CO TU JEST:
 *    • Struktury konfiguracyjne dla napędu, TF-Luna, TCS3472 oraz prosty Scheduler.
 *    • Jawne, czytelne pola z komentarzami „po co / jak używane”.
 *    • Gettery CFG_*() zwracające const-wskaźniki na statyczne struktury w config.c.
 *
 *  PO CO:
 *    • Jedno źródło prawdy dla parametrów strojenia (bez rozlewania #define po plikach).
 *    • Możliwość strojenia rytmów i progów bez zmiany logiki w modułach.
 *
 *  WAŻNE USTALENIA (zgodnie z projektem):
 *    • NIE zmieniamy nazw typów: ConfigMotors_t / ConfigLuna_t / ConfigTCS_t oraz enum TCS_Gain_t.
 *    • Pola w ConfigMotors_t pozostają takie, jak w Twoim kodzie (w tym left/right_scale,
 *      esc_start_pct / esc_max_pct, smooth_alpha itd.).
 *    • Dodatkowo wprowadzony jest lekki Scheduler: ConfigScheduler_t + CFG_Scheduler().
 *      W app.c używamy nadal makr PERIOD_* — źródłem wartości jest CFG_Scheduler().
 * ============================================================================
 */

#include <stdint.h>

/* ===============================
 *  Napęd / Tank drive / Rampy
 * =============================== */
typedef struct
{
    /* Rytm aktualizacji pętli napędu (ms) — np. 20 ms ⇒ 50 Hz */
    uint16_t tick_ms;

    /* Krótki neutral przy zmianie kierunku (ms) — realizowany nieblokująco w logice reverse-gate */
    uint16_t neutral_dwell_ms;

    /* Maksymalny krok zmiany mocy na tick [%] — „połówkowa” rampa startu/hamowania */
    uint8_t  ramp_step_pct;

    /* Próg rozpoznania zmiany kierunku [%] — eliminuje wahania wokół zera */
    uint8_t  reverse_threshold_pct;

    /* Współczynnik EMA dla wygładzania zmian (0..1) — np. 0.25f */
    float    smooth_alpha;

    /* Kompensacja asymetrii torów (1.00 = brak korekty) */
    float    left_scale;    /* skala lewego kanału */
    float    right_scale;   /* skala prawego kanału */

    /* Okno „użytecznego” zakresu ESC [%] — nasz 0..100% mapuje do (esc_start..esc_max) */
    uint8_t  esc_start_pct; /* np. 30 — wyjście z martwej strefy */
    uint8_t  esc_max_pct;   /* np. 60 — „nasz sufit” mocy */

} ConfigMotors_t;

/* ==========================
 *  TF-Luna (I²C) — filtry
 * ========================== */
typedef struct
{
    /* Mediana okna N — redukuje pojedyncze skoki */
    uint8_t  median_win;

    /* Średnia krocząca okna N — wygładza trend */
    uint8_t  ma_win;

    /* Skala temperatury (1.0 = bez zmian) — jeśli driver raportuje „surową” wartość */
    float    temp_scale;

    /* Korekta offsetu dystansu [mm] po kalibracji (prawy / lewy sensor) */
    int16_t  dist_offset_right_mm;
    int16_t  dist_offset_left_mm;

} ConfigLuna_t;

/* ===========================================
 *  TCS3472 (I²C) — integracja i wzmocnienie
 * =========================================== */

/* Wzmocnienie (AGAIN) TCS3472 — zgodnie z rejestrem: 0x00=1x, 0x01=4x, 0x02=16x, 0x03=60x */
typedef enum
{
    TCS_GAIN_1X  = 0x00,
    TCS_GAIN_4X  = 0x01,
    TCS_GAIN_16X = 0x02,
    TCS_GAIN_60X = 0x03
} TCS_Gain_t;

typedef struct
{
    /* Czas integracji (ms) — kompromis czułość/odświeżanie */
    uint16_t   atime_ms;

    /* Wzmocnienie (AGAIN) — patrz TCS_Gain_t */
    TCS_Gain_t gain;

} ConfigTCS_t;

/* ==========================
 *  Scheduler (okresy zadań)
 * ========================== */
typedef struct
{
    /* co ile ms odświeżać sensory (TF-Luna/TCS) */
    uint16_t sens_ms;

    /* co ile ms odświeżać OLED */
    uint16_t oled_ms;

    /* co ile ms odświeżać panel UART */
    uint16_t uart_ms;

} ConfigScheduler_t;

/* ====================
 *  GETTERY CFG_*()
 * ==================== */
#ifdef __cplusplus
extern "C" {
#endif

/* Napęd / rampy / okno ESC / kompensacja torów */
const ConfigMotors_t*     CFG_Motors(void);

/* TF-Luna — filtry i offsety */
const ConfigLuna_t*       CFG_Luna(void);

/* TCS3472 — integracja + gain */
const ConfigTCS_t*        CFG_TCS(void);

/* Scheduler — okresy zadań (źródło dla PERIOD_*) */
const ConfigScheduler_t*  CFG_Scheduler(void);

#ifdef __cplusplus
}
#endif
