/*
 * =============================================================================
 *  MODULE: config.h — Jedno źródło prawdy dla parametrów projektu DzikiBoT
 * -----------------------------------------------------------------------------
 *  CO:
 *    - Struktury konfiguracyjne (Motors, TF-Luna, TCS3472, Scheduler) + enum gainu.
 *    - Gettery CFG_*() oraz opcjonalne gettery tuningu TCS (EMA + progi auto-gain).
 *
 *  JAK UŻYWAĆ:
 *    - Wartości domyślne edytuj w config.c (g_motors/g_luna/g_tcs/g_sched).
 *    - Moduły czytają WYŁĄCZNIE przez: CFG_Motors(), CFG_Luna(), CFG_TCS(), CFG_Scheduler().
 *    - Tuning TCS w runtime (bez zmiany struktur): nadpisz w config.c
 *      CFG_TCS_EMA_Alpha(), CFG_TCS_AG_LoPct(), CFG_TCS_AG_HiPct().
 *
 *  QUICK REF — typowe zakresy (skrót do podejrzenia „na gorąco”):
 *  ─ Motors/Tank: tick_ms 10..50 (typ 20), ramp_step 1..10, dwell 200..800,
 *                 reverse_gate 1..5, smooth_alpha 0.10..0.40,
 *                 left/right_scale 0.90..1.10, esc_start 20..40, esc_max 50..80.
 *  ─ TF-Luna    : median 1..7, ma_win 1..8, temp_offset −30..+10 °C,
 *                 dist_offset_[L/R] −200..+200 mm.
 *  ─ TCS3472    : atime 24..154 ms, gain 1×/4×/16×/60× (auto-gain przejmuje dalej),
 *                 EMA_Alpha 0.10..0.50, Lo% 0.05..0.90, Hi% 0.10..0.95 (Hi ≥ Lo+0.02).
 *  ─ Scheduler  : sens_ms 50..200, oled_ms 100..500, uart_ms 100..500.
 *
 *  MINI-FAQ STROJENIA:
 *  1) Szarpie przy starcie → ramp_step↓, smooth_alpha↓, esc_start↑.
 *  2) Reverse wchodzi wolno → dwell↓, reverse_gate↓.
 *  3) Ściąga na prostej → left/right_scale koryguj po 0.01.
 *  4) OLED dusi pętlę → oled_ms↑ / stronicuj / rozsuń z UART; I²C Fm+ pomaga.
 *  5) Jitter > 40 ms → skróć time-outy I²C (3–5 ms), rozsuń zadania, sprawdź I²C.
 *  6) TCS nasyca się → gain start↓ lub Hi%↓/Lo%↓; TCS wolno/skacze → EMA_Alpha↑/↓.
 * =============================================================================
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>

/* ==== TCS3472: poziomy gain ==== */
typedef enum {
    TCS_GAIN_1X  = 0,
    TCS_GAIN_4X  = 1,
    TCS_GAIN_16X = 2,
    TCS_GAIN_60X = 3
} TCS_Gain_t;

/* ==== MOTORS / TANK DRIVE ==== */
typedef struct {
    uint32_t tick_ms;                // rytm Tank_Update (np. 20 ms → 50 Hz)
    uint32_t neutral_dwell_ms;       // czas neutralu przy zmianie kierunku (reverse-gate)
    uint8_t  ramp_step_pct;          // krok rampy w %/tick
    uint8_t  reverse_threshold_pct;  // próg „bramki” przy przejściu przez 0% (anty-oscylacje)
    float    smooth_alpha;           // EMA na wejściu sterowania (0..1)
    float    left_scale;             // korekta lewego toru (1.00 = brak)
    float    right_scale;            // korekta prawego toru (1.00 = brak)
    uint8_t  esc_start_pct;          // początek okna ESC (wyjście z martwej strefy)
    uint8_t  esc_max_pct;            // koniec okna ESC (nasze „100% mocy”)
} ConfigMotors_t;

/* ==== TF-LUNA ==== */
typedef struct {
    uint8_t  median_win;             // okno mediany (odporność na piki)
    uint8_t  ma_win;                 // okno średniej kroczącej (trend)
    float    temp_scale;             // skala temperatury (zwykle 1.0)
    float    temp_offset_c;          // offset ambientu względem temp. układu (°C)
    int16_t  dist_offset_right_mm;   // offset dystansu (prawy) [mm]
    int16_t  dist_offset_left_mm;    // offset dystansu (lewy) [mm]
} ConfigLuna_t;

/* ==== TCS3472 ==== */
typedef struct {
    uint16_t   atime_ms;             // czas integracji ADC (ms)
    TCS_Gain_t gain;                 // gain startowy (auto-gain przejmie po starcie)
} ConfigTCS_t;

/* ==== SCHEDULER ==== */
typedef struct {
    uint16_t sens_ms;                // okres odczytów sensorów
    uint16_t oled_ms;                // okres odświeżania OLED
    uint16_t uart_ms;                // okres odświeżania panelu UART
} ConfigScheduler_t;

/* ==== Gettery (jedyny sposób dostępu w kodzie) ==== */
const ConfigMotors_t*     CFG_Motors(void);
const ConfigLuna_t*       CFG_Luna(void);
const ConfigTCS_t*        CFG_TCS(void);
const ConfigScheduler_t*  CFG_Scheduler(void);

/* ==== OPCJONALNY TUNING TCS (override „weak” w driverze) ======================
 *  Zakresy:
 *    CFG_TCS_EMA_Alpha(): 0.10..0.50 (typ 0.30)
 *    CFG_TCS_AG_LoPct() : 0.05..0.90 (typ 0.60)
 *    CFG_TCS_AG_HiPct() : 0.10..0.95 (typ 0.70)  (Hi ≥ Lo+0.02)
 *  Zdefiniuj te funkcje w config.c, aby runtime nadpisać domyślne wartości.
 *  Usuniesz je z config.c → driver użyje swoich „weak” default.
 * =========================================================================== */
float CFG_TCS_EMA_Alpha(void);
float CFG_TCS_AG_LoPct(void);
float CFG_TCS_AG_HiPct(void);

#endif /* CONFIG_H_ */
