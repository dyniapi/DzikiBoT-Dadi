/**
 * @file    config.c
 * @brief   Centralna konfiguracja – implementacja (blok: SILNIKI)
 */

#include "config.h"

/* Domyślna, spójna konfiguracja napędu */
static const ConfigMotors_t g_cfg_motors = {
  .tick_ms       = 20,     /* Tank_Update co 20 ms */
  .ramp_step_pct = 4,      /* 4% / tick → ~200%/s przy 20 ms */
  .smooth_alpha  = 0.20f,  /* łagodne EMA */
  .left_scale    = 1.00f,  /* dostroisz po montażu kół */
  .right_scale   = 1.00f,
  .esc_start_pct = 30,     /* „start” pracy ESC */
  .esc_max_pct   = 60,      /* „nasze 100%” = 60% ESC-PWM */
  .neutral_dwell_ms     = 500,  /* 0.5 s twardy neutral przy FWD<->REV */
  .reverse_threshold_pct= 5,    /* zmiana znaku liczy się dopiero powyżej 5% */
};

const ConfigMotors_t* CFG_Motors(void)
{
  return &g_cfg_motors;
}



/* ==== [LUNA] ==== */
static const ConfigLuna_t g_cfg_luna = {
  .update_ms           = 100,    // spójne z tym, co masz w pętli
  .median_win          = 5,      // MED5 na dystans/siłę
  .ma_win              = 5,      // MA5 (ruchome uśrednianie)
  .no_frame_timeout_ms = 500,    // po 0.5 s pokaż „NO FRAME”
  .strength_min        = 50,     // poniżej – odrzuć/oznacz słaby odczyt
  .dist_min_mm         = 30,     // ignoruj bardzo bliskie śmieci
  .dist_max_mm         = 2000,   // i zbyt dalekie
  .temp_scale          = 1.0f,   // TF-Luna temp = raw * 0.1
  .dist_offset_right_mm= 0,      // kalibracja na przyszłość
  .dist_offset_left_mm = 0
};

const ConfigLuna_t* CFG_Luna(void) { return &g_cfg_luna; }

/* ==== [TCS3472] ==== */
static const ConfigTCS_t g_cfg_tcs = {
  .update_ms       = 100,     // żeby iść równo z Luną (możesz zmienić)
  .atime_ms        = 50.0f,   // kompromis: nie za długie, nie za krótkie
  .gain            = TCS_GAIN_16X,
  .rgb_divisor     = 64,      // jak dotąd dzieliłeś /64
  .clear_white_thr = 1200,    // dobierz na ringu po kalibracji
  .clear_black_thr = 400,     // jw.
  .edge_debounce   = 3,       // trzy próbki, żeby potwierdzić
  .red_scale       = 1.00f,   // na wypadek różnic między modułami
  .green_scale     = 1.00f,
  .blue_scale      = 1.00f
};

const ConfigTCS_t* CFG_TCS(void) { return &g_cfg_tcs; }
