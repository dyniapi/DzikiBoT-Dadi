/**
 * @file    config.h
 * @brief   Centralna konfiguracja projektu (blok S I L N I K I)
 * @date    2025-10-28
 *
 * Uwaga: ten plik będzie rozbudowany o kolejne bloki:
 *  - [TF_LUNA]
 *  - [TCS3472]
 * Na razie zawiera tylko blok [MOTORS].
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== [MOTORS] ===================== */
/* Wszystkie parametry strojenia napędu w jednym miejscu */
typedef struct {
  /* TIM1 mapowanie 1–2 ms jest w motor_bldc (liniowe).
     Tu definiujemy logikę dla tank_drive: rampa/kompensacja/wygładzanie. */

  /* Częstotliwość wywołania Tank_Update (ms) – referencyjna informacja */
  uint16_t tick_ms;

  /* Rampa: maks. zmiana % (−100..100) jednego koła na 1 tick */
  uint8_t  ramp_step_pct;

  /* Wygładzanie (EMA) wyjścia po rampie, per koło (0.0=off, 0.1..0.5 aktywne) */
  float    smooth_alpha;  /* np. 0.20f → ~łagodne wygładzenie */

  /* Kompensacja różnic kół (skalowanie po rampie i wygładzeniu) */
  float    left_scale;    /* 1.00..1.10 typowo */
  float    right_scale;   /* 1.00..1.10 typowo */

  /* Okno „użyteczne” ESC-PWM (wyrażone jako procent „gazu” ESC):
     - 30% = punkt START (silnik zaczyna pewnie pracować; duży moment),
     - 60% = MAX (uznajemy to za nasze 100% komendy).
     Tank_drive przemapowuje |cmd| z [0..100] → [START..MAX].
     Znak cmd wybiera kierunek (poniżej/ powyżej 1500 µs). */
  uint8_t  esc_start_pct; /* domyślnie 30 */
  uint8_t  esc_max_pct;   /* domyślnie 60 */

  /* Neutral dwell dla ESC „crawler”: ile ms trzymać 0% przy zmianie kierunku */
  uint16_t neutral_dwell_ms;  /* np. 500 ms */

  /* Próg wykrycia „istotnej” zmiany znaku (aby drobne fluktuacje nie wyzwalały dwell) */
  uint8_t  reverse_threshold_pct; /* np. 5% */



} ConfigMotors_t;

/* Dostęp do egzemplarza konfigu */
const ConfigMotors_t* CFG_Motors(void);



/* ===================== [LUNA] ===================== */
typedef struct {
  /* Okres odczytu (ms) – jeśli chcesz scentralizować timing sensorów */
  uint16_t update_ms;

  /* Filtry na dystans/siłę – okna (0 = wyłączone) */
  uint8_t  median_win;     // MUSI być liczbą nieparzystą: 3,5,7...
  uint8_t  ma_win;         // Moving Average (np. 5)

  /* Progi/diagnostyka */
  uint16_t no_frame_timeout_ms;  // po tylu ms oznacz „NO FRAME”
  uint16_t strength_min;         // minimalna siła akceptacji ramki
  uint16_t dist_min_mm;          // minimalny dystans użyteczny (filtrowanie)
  uint16_t dist_max_mm;          // maksymalny dystans użyteczny

  /* Skalowanie/kalibracja temperatury i dystansu */
  float    temp_scale;           // TF-Luna: surowa temp * scale (zwykle 0.1f)
  int16_t  dist_offset_right_mm; // korekta (Right)
  int16_t  dist_offset_left_mm;  // korekta (Left)
} ConfigLuna_t;

const ConfigLuna_t* CFG_Luna(void);





/* ===================== [TCS3472] ===================== */
typedef enum {
  TCS_GAIN_1X  = 0,
  TCS_GAIN_4X  = 1,
  TCS_GAIN_16X = 2,
  TCS_GAIN_60X = 3
} TCS_Gain_t;

typedef struct {
  /* Okres odczytu (ms) – jeśli centralizujesz timing */
  uint16_t update_ms;

  /* Ustawienia rejestrów w logice – driver dobierze najbliższe */
  float     atime_ms;     // typowo: 2.4, 24, 50, 101, 154, 700
  TCS_Gain_t gain;        // 1x/4x/16x/60x

  /* Normalizacja/skalowanie wyświetlania */
  uint16_t rgb_divisor;   // np. 64 (jak wcześniej dzieliłeś)

  /* Progi „bieli/czerni” pod ring (dohyo) + histereza/debounce */
  uint16_t clear_white_thr;  // powyżej = białe (krawędź ringu)
  uint16_t clear_black_thr;  // poniżej = czarne
  uint8_t  edge_debounce;    // ile kolejnych próbek potwierdza wykrycie

  /* Prosta kalibracja kanałów (jeśli kości mają różnice) */
  float red_scale;
  float green_scale;
  float blue_scale;
} ConfigTCS_t;

const ConfigTCS_t* CFG_TCS(void);




#ifdef __cplusplus
}
#endif
#endif /* CONFIG_H */
