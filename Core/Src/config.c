/*
 * ============================================================================
 *  DzikiBoT — wartości domyślne konfiguracji
 *  ----------------------------------------------------------------------------
 *  Uwaga:
 *    • To są „gałki” do strojenia zachowania — logika modułów ich nie zmienia.
 *    • Zmiana wartości tutaj nie wymaga dotykania pozostałych plików.
 * ============================================================================
 */

#include "config.h"

/* ===============================
 *  Napęd / Tank drive / Rampy
 * =============================== */
static const ConfigMotors_t g_motors = {
    /* Rytm napędu (ms) — 20 ms ⇒ 50 Hz */
    .tick_ms               = 20,

    /* Krótki neutral przy zmianie kierunku (ms) — reverse-gate w Tank_Update */
    .neutral_dwell_ms      = 600,

    /* Rampa [%/tick] — łagodne starty/hamowania */
    .ramp_step_pct         = 4,

    /* Próg [%] zmiany kierunku (eliminuje drgania wokół zera) */
    .reverse_threshold_pct = 3,

    /* Wygładzanie EMA (0..1) — 0.25f to umiarkowane filtrowanie zmian */
    .smooth_alpha          = 0.25f,

    /* Kompensacja torów (1.00 = brak korekty) */
    .left_scale            = 1.00f,
    .right_scale           = 1.00f,

    /* Okno „użyteczne” ESC [%] — nasz 0..100% mapuje do 30..60% rzeczywistego zakresu ESC
     * (łatwiejsza i „mięsista” kontrola momentu przy niskich prędkościach — minisumo)
     */
    .esc_start_pct         = 30,
    .esc_max_pct           = 60,
};

/* ==========================
 *  TF-Luna (I²C) — filtry
 * ========================== */
static const ConfigLuna_t g_luna = {
    .median_win             = 3,      /* mediana z 3 próbek */
    .ma_win                 = 4,      /* średnia krocząca z 4 próbek */
    .temp_scale             = 1.0f,   /* bez zmiany skali temperatury */
    .dist_offset_right_mm   = 0,      /* ustaw po kalibracji sceny, jeśli trzeba */
    .dist_offset_left_mm    = 0,
};

/* ===========================================
 *  TCS3472 (I²C) — integracja i wzmocnienie
 * =========================================== */
static const ConfigTCS_t g_tcs = {
    .atime_ms = 100,            /* 50–154 ms typowo; 100 ms to dobry punkt startowy */
    .gain     = TCS_GAIN_16X,   /* startowo 16x — dobra czułość w typowych warunkach */
};

/* ==========================
 *  Scheduler (okresy zadań)
 * ========================== */
static const ConfigScheduler_t g_sched = {
    .sens_ms = 100,   /* było PERIOD_SENS_MS  = 100U */
    .oled_ms = 200,   /* było PERIOD_OLED_MS  = 200U */
    .uart_ms = 200,   /* było PERIOD_UART_MS  = 200U */
};

/* ================
 *  GETTERY CFG_*()
 * ================ */

const ConfigMotors_t*     CFG_Motors(void)    { return &g_motors; }
const ConfigLuna_t*       CFG_Luna(void)      { return &g_luna;   }
const ConfigTCS_t*        CFG_TCS(void)       { return &g_tcs;    }
const ConfigScheduler_t*  CFG_Scheduler(void) { return &g_sched;  }
