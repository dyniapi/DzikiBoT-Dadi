/*
 * =============================================================================
 *  MODULE: config.h — Jedno źródło prawdy dla parametrów projektu DzikiBoT
 * -----------------------------------------------------------------------------
 *  CO:
 *    - Struktury konfiguracyjne (Motors, TF-Luna, TCS3472, Scheduler).
 *    - Enum TCS_Gain_t.
 *    - Prototypy getterów CFG_*() oraz (opcjonalnie) getterów tuningu TCS.
 *
 *  JAK CZYTAĆ:
 *    - Wartości domyślne są w config.c — tylko tam stroimy.
 *    - Moduły czytają WYŁĄCZNIE przez CFG_*() (łatwy refactor).
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
    uint32_t neutral_dwell_ms;       // czas neutralu przy zmianie kierunku
    uint8_t  ramp_step_pct;          // krok rampy w %/tick
    uint8_t  reverse_threshold_pct;  // „bramka” przejścia przez 0% (eliminuje oscylacje)
    float    smooth_alpha;           // EMA na wejściu sterowania (0..1)
    float    left_scale;             // korekta lewego toru (1.00 = brak)
    float    right_scale;            // korekta prawego toru (1.00 = brak)
    uint8_t  esc_start_pct;          // początek „okna” ESC (wyjście z martwej strefy)
    uint8_t  esc_max_pct;            // koniec „okna” ESC (nasze „100% mocy”)
} ConfigMotors_t;

/* ==== TF-LUNA ==== */
typedef struct {
    uint8_t  median_win;             // okno mediany (odporność na piki)
    uint8_t  ma_win;                 // okno średniej kroczącej (trend)
    float    temp_scale;             // skala temperatury (zwykle 1.0)
    float    temp_offset_c;          // offset ambientu względem temp. układu
    int16_t  dist_offset_right_mm;   // offset dystansu (prawy)
    int16_t  dist_offset_left_mm;    // offset dystansu (lewy)
} ConfigLuna_t;

/* ==== TCS3472 ==== */
typedef struct {
    uint16_t   atime_ms;             // czas integracji (≈ czułość)
    TCS_Gain_t gain;                 // gain startowy (auto-gain przejmie po starcie)
} ConfigTCS_t;

/* ==== SCHEDULER ==== */
typedef struct {
    uint16_t sens_ms;                // rytm sensorów (TF-Luna + TCS)
    uint16_t oled_ms;                // rytm OLED
    uint16_t uart_ms;                // rytm UART
} ConfigScheduler_t;

/* ==== Gettery (jedyny sposób dostępu) ==== */
const ConfigMotors_t*     CFG_Motors(void);
const ConfigLuna_t*       CFG_Luna(void);
const ConfigTCS_t*        CFG_TCS(void);
const ConfigScheduler_t*  CFG_Scheduler(void);

/* ============================================================================
 *  OPCJONALNE GETTERY TUNINGU TCS (override „weak” z drivera — bez zmiany struktur)
 *  Zakresy:
 *    CFG_TCS_EMA_Alpha(): 0.10..0.50 (typ. 0.30)
 *    CFG_TCS_AG_LoPct() : 0.05..0.90 (typ. 0.60)
 *    CFG_TCS_AG_HiPct() : 0.10..0.95 (typ. 0.70)  (Hi > Lo o ≥ 0.02)
 * ========================================================================== */
float CFG_TCS_EMA_Alpha(void);
float CFG_TCS_AG_LoPct(void);
float CFG_TCS_AG_HiPct(void);

#endif /* CONFIG_H_ */
