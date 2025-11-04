/*
 * =============================================================================
 *  DzikiBoT — DOMYŚLNE wartości konfiguracji (strojenie w jednym miejscu)
 * -----------------------------------------------------------------------------
 *  CO TU JEST:
 *    • Zestaw „gałek” dla: TankDrive (Motors), TF-Luna, TCS3472, Scheduler.
 *    • Gettery CFG_*() — moduły czytają TYLKO przez nie (jedno źródło prawdy).
 *    • (Opcjonalne) gettery tuningu TCS (EMA + progi auto-gain) — override „weak”.
 *
 *  JAK UŻYWAĆ:
 *    • Zmieniasz wartości w stałych poniżej (g_motors/g_luna/g_tcs/g_sched).
 *    • Kod czyta je przez: CFG_Motors(), CFG_Luna(), CFG_TCS(), CFG_Scheduler().
 *    • Tuning TCS w runtime bez ruszania struktur: nadpisz CFG_TCS_EMA_Alpha(),
 *      CFG_TCS_AG_LoPct(), CFG_TCS_AG_HiPct() (są na dole tego pliku).
 *
 *  QUICK REF — typowe zakresy strojenia (skrót przydatny „na gorąco”):
 *  ────────────────────────────────────────────────────────────────────────────
 *  [Motors / TankDrive]
 *    tick_ms               : 10..50        (typ. 20 → 50 Hz)
 *    neutral_dwell_ms      : 200..800      (typ. 600 | u nas 100 dla żwawości)
 *    ramp_step_pct         : 1..10         (większe = żwawiej, mniejsze = łagodniej)
 *    reverse_threshold_pct : 1..5          (eliminuje oscylacje przy 0%)
 *    smooth_alpha          : 0.10..0.40    (EMA; większe = mniej „gumowe” sterowanie)
 *    left_scale/right_scale: 0.90..1.10    (balans torów; kroki ±0.01)
 *    esc_start_pct         : 20..40        (wyjście z martwej strefy)
 *    esc_max_pct           : 50..80        (gdzie „100%” w naszej mapie)
 *
 *  [TF-Luna]
 *    median_win            : 1..7          (odporność na piki)
 *    ma_win                : 1..8          (trend; większe = gładsze, wolniejsze)
 *    temp_scale            : 0.5..2.0
 *    temp_offset_c         : −30..+10 °C   (estymacja ambientu vs temp. układu)
 *    dist_offset_[LR]_mm   : −200..+200 mm (kalibracja geometrii)
 *
 *  [TCS3472]
 *    atime_ms              : 24..154 ms    (większe = czulszy, wolniejszy)
 *    gain (start)          : 1× / 4× / 16× / 60×  (auto-gain przejmie ster po starcie)
 *    EMA_Alpha             : 0.10..0.50    (CFG_TCS_EMA_Alpha; typ. 0.30)
 *    Auto-gain Lo%         : 0.05..0.90    (CFG_TCS_AG_LoPct; typ. 0.60)
 *    Auto-gain Hi%         : 0.10..0.95    (CFG_TCS_AG_HiPct; typ. 0.70; Hi ≥ Lo+0.02)
 *
 *  [Scheduler]
 *    sens_ms               : 50..200 ms    (odczyty sensorów)
 *    oled_ms               : 100..500 ms   (odświeżanie OLED)
 *    uart_ms               : 100..500 ms   (odświeżanie panelu UART)
 *  ────────────────────────────────────────────────────────────────────────────
 *
 *  FAQ STROJENIA (szybkie podpowiedzi):
 *  1) „Szarpie przy starcie/zmianach”:
 *       → ramp_step_pct↓ (np. 6→4), smooth_alpha↓ (0.25→0.20), esc_start_pct↑ (20→28).
 *  2) „Reverse wchodzi wolno / długo stoi w neutralu”:
 *       → neutral_dwell_ms↓ (600→350), reverse_threshold_pct↓ (3→2).
 *  3) „Ściąga na prostej”:
 *       → left_scale/right_scale dopasuj w krokach ±0.01 aż do jazdy wprost.
 *  4) „OLED miga / dławi pętlę”:
 *       → oled_ms↑ (np. 200→300/400), rozważ stronicowanie ekranu; I²C 1 MHz pomaga.
 *  5) „Jitter Tank > 40 ms, sporadyczne piki 80–100 ms”:
 *       → skróć timeouty I²C w driverach (3–5 ms zamiast 20 ms),
 *         rozsuń zadania (OLED vs UART), rozfazuj sensory (już zrobione),
 *         ewentualnie I²C Fast-mode Plus (1 MHz) jeśli hardware pozwala.
 *  6) „TCS się nasyca (Clear przy górnej granicy)”:
 *       → gain startowy↓ (np. 16×→4×), albo ustaw CFG_TCS_AG_HiPct na 0.65,
 *         CFG_TCS_AG_LoPct na 0.55 (mniejsza histereza, szybsze zejście z gainu).
 *  7) „TCS wolno reaguje / skacze kolor”:
 *       → EMA_Alpha↑ (0.30→0.40) — szybciej; lub ↓ (0.30→0.20) — stabilniej.
 *
 *  BEZPIECZEŃSTWO:
 *    • Ten plik nie zawiera opóźnień blokujących. Jedyne blokujące miejsce w projekcie
 *      to arming ESC (neutral ~3 s) w App_Init. Pozostałe pętle są nieblokujące.
 * =============================================================================
 */

#include "config.h"

/* ==== MOTORS / TANK DRIVE ==== */
static const ConfigMotors_t g_motors = {
    .tick_ms               = 20,     // 20 ms → 50 Hz (responsywne i stabilne)
    .neutral_dwell_ms      = 100,    // ms neutralu przy zmianie kierunku (reverse-gate)
    .ramp_step_pct         = 6,      // %/tick – większe = żwawiej, mniejsze = łagodniej
    .reverse_threshold_pct = 2,      // % – „bramka” przejścia przez 0% (eliminuje oscylacje)
    .smooth_alpha          = 0.25f,  // [0..1] EMA na wejściu sterowania
    .left_scale            = 1.00f,  // × korekta lewego toru
    .right_scale           = 1.00f,  // × korekta prawego toru
    .esc_start_pct         = 20,     // % – wyjście z martwej strefy ESC
    .esc_max_pct           = 60,     // % – nasze „100% mocy” (mapowanie %→µs)
};

/* ==== TF-LUNA ==== */
static const ConfigLuna_t g_luna = {
    .median_win             = 3,      // okno mediany
    .ma_win                 = 4,      // okno średniej kroczącej
    .temp_scale             = 1.0f,   // skala temp.
    .temp_offset_c          = -25.0f, // °C: przybliżony offset do ambientu vs temp. układu
    .dist_offset_right_mm   = 0,      // mm offset (prawy)
    .dist_offset_left_mm    = 0,      // mm offset (lewy)
};

/* ==== TCS3472 ==== */
static const ConfigTCS_t g_tcs = {
    .atime_ms = 100,            // ms integracji: dobry punkt startowy (nie za wolno/za szybko)
    .gain     = TCS_GAIN_16X,   // start gain (auto-gain i tak przejmie sterowanie)
};

/* ==== SCHEDULER ==== */
static const ConfigScheduler_t g_sched = {
    .sens_ms = 100,   // ms: odczyt sensorów (TF-Luna + TCS)
    .oled_ms = 200,   // ms: odświeżanie OLED
    .uart_ms = 200,   // ms: odświeżanie panelu UART
};

/* ==== Gettery CFG_*() — jedyny sposób dostępu dla modułów ==== */
const ConfigMotors_t*     CFG_Motors(void)    { return &g_motors; }
const ConfigLuna_t*       CFG_Luna(void)      { return &g_luna;   }
const ConfigTCS_t*        CFG_TCS(void)       { return &g_tcs;    }
const ConfigScheduler_t*  CFG_Scheduler(void) { return &g_sched;  }

/* =============================================================================
 *  TCS — tuning runtime (EMA + progi auto-gain) przez gettery (override „weak”)
 *  ────────────────────────────────────────────────────────────────────────────
 *  • Zdefiniowane tutaj → driver TCS użyje tych wartości (zamiast weak default).
 *  • Usuniesz je → driver użyje swoich domyślnych (alpha=0.30, lo=0.60, hi=0.70).
 *  • Histereza min. 2 p.p. (Hi ≥ Lo+0.02) wymuszana jest po stronie drivera.
 * =============================================================================
 */
float CFG_TCS_EMA_Alpha(void) { return 0.30f; }  // 0..1 — większe = szybsza reakcja, mniejsze = gładsza

float CFG_TCS_AG_LoPct(void)  { return 0.60f; }  // 0..1 — dolny próg (Clear ≈ 60% FS)

float CFG_TCS_AG_HiPct(void)  { return 0.70f; }  // 0..1 — górny próg (Clear ≈ 70% FS)
