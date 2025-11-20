/*
 * =============================================================================
 *  DzikiBoT — DOMYŚLNE wartości konfiguracji (strojenie w jednym miejscu)
 * -----------------------------------------------------------------------------
 *  CO TU JEST:
 *    • Zestaw „gałek” dla: TankDrive, TF-Luna, TCS3472, Scheduler.
 *    • Gettery CFG_*() — moduły czytają TYLKO przez nie.
 *    • (Nowe) gettery tuningu TCS (EMA + progi auto-gain) — override „weak”.
 *
 *  QUICK REF (typowe zakresy):
 *  [Motors] tick_ms:10..50 | ramp_step:1..10 | neutral_dwell:200..800 | smooth_alpha:0.10..0.40
 *  [Luna]   median:1..7 | ma:1..8 | temp_offset_c:~−30..+10
 *  [TCS]    atime:24..154 ms | start gain:1×/4×/16×/60× | tuning: CFG_TCS_*()
 *  [Sched]  sens:50..200 | oled:100..500 | uart:100..500
 * =============================================================================
 */

#include "config.h"

/* ==== MOTORS / TANK DRIVE ==== */
static const ConfigMotors_t g_motors = {
    .tick_ms               = 20,     // 20 ms → 50 Hz (responsywne i stabilne)
    .neutral_dwell_ms      = 100,    // ms neutralu przy zmianie kierunku
    .ramp_step_pct         = 6,      // %/tick – większe = żwawiej, mniejsze = łagodniej
    .reverse_threshold_pct = 2,      // % – eliminuje oscylacje przy 0%
    .smooth_alpha          = 0.25f,  // [0..1] EMA na wejściu sterowania
    .left_scale            = 1.00f,  // × korekta lewego toru
    .right_scale           = 1.00f,  // × korekta prawego toru
    .esc_start_pct         = 20,     // % – wyjście z martwej strefy ESC
    .esc_max_pct           = 60,     // % – nasze „100% mocy”
};

/* ==== TF-LUNA ==== */
static const ConfigLuna_t g_luna = {
    .median_win             = 3,      // okno mediany
    .ma_win                 = 4,      // okno średniej kroczącej
    .temp_scale             = 1.0f,   // skala temp.
    .temp_offset_c          = -25.0f, // °C: przybliżony offset do ambientu
    .dist_offset_right_mm   = 0,      // mm offset (prawy)
    .dist_offset_left_mm    = 0,      // mm offset (lewy)
};

/* ==== TCS3472 ==== */
static const ConfigTCS_t g_tcs = {
    .atime_ms = 100,            // ms integracji: dobry punkt startowy
    .gain     = TCS_GAIN_16X,   // start gain (auto-gain dalej steruje)
};

/* ==== SCHEDULER ==== */
static const ConfigScheduler_t g_sched = {
    .sens_ms = 100,   // ms: odczyt sensorów
    .oled_ms = 200,   // ms: odświeżanie OLED
    .uart_ms = 200,   // ms: odświeżanie UART
};

/* ==== Gettery CFG_*() ==== */
const ConfigMotors_t*     CFG_Motors(void)    { return &g_motors; }
const ConfigLuna_t*       CFG_Luna(void)      { return &g_luna;   }
const ConfigTCS_t*        CFG_TCS(void)       { return &g_tcs;    }
const ConfigScheduler_t*  CFG_Scheduler(void) { return &g_sched;  }

/* =============================================================================
 *  TCS — tuning runtime (EMA + progi auto-gain) przez gettery (override „weak”)
 *  • Zdefiniowane tutaj → driver TCS użyje tych wartości.
 *  • Usuniesz je → driver użyje swoich domyślnych (alpha=0.30, lo=0.60, hi=0.70).
 *  • Histereza min. 2 p.p. wymuszana jest po stronie drivera.
 * =============================================================================
 */
float CFG_TCS_EMA_Alpha(void) { return 0.30f; }  // 0..1 — większe = szybciej, mniejsze = gładszy

float CFG_TCS_AG_LoPct(void)  { return 0.60f; }  // 0..1 — dolny próg (Clear ≈ 60% FS)

float CFG_TCS_AG_HiPct(void)  { return 0.70f; }  // 0..1 — górny próg (Clear ≈ 70% FS)
