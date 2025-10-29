/**
 * @file   throttle_map.h
 * @brief  Kompensacja różnic L/R + nieliniowa krzywa przepustnicy
 * @date   2025-10-28
 *
 * Użycie:
 *   Throttle_Params_t p = THROTTLE_DEFAULTS;
 *   // dostrój:
 *   p.left.scale  = 1.07f;   // 7% mocniej lewy
 *   p.right.scale = 1.00f;
 *   p.curve.gamma = 1.6f;    // „zmiękcza” dół, wyrównuje skok koło 50%
 *   Throttle_Init(&p);
 *
 *   int8_t l_out = Throttle_Apply(-30, THR_LEFT);  // -100..100 wejście
 *   int8_t r_out = Throttle_Apply(+30, THR_RIGHT); // -100..100 wyjście (po kompensacji)
 */

#ifndef THROTTLE_MAP_H
#define THROTTLE_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { THR_LEFT = 0, THR_RIGHT = 1 } ThrSide_t;

typedef struct {
  float scale;   /* mnożnik mocy koła (0.80..1.20 typowo) */
  float offset;  /* dodatek w % po skali, zwykle 0.0 */
} ThrTrim_t;

typedef struct {
  float gamma;        /* >1.0 = miękko przy małych wartościach (polecane 1.4..2.0) */
  float deadband;     /* martwa strefa wejściowa [%], np. 3.0 */
  float out_limit;    /* maks. wyjście po krzywej (<=100), np. 100 */
  /* opcjonalny „shoulder” – delikatne dociążenie środka, by przeskoczyć szarpnięcie:
     przy 30% (crawler) często jest „spike”, więc lekkie spłaszczenie (S-curve) pomaga */
  float shoulder_pct; /* 0..100, np. 50 oznacza że ok. 50% wejścia wygładzamy bardziej */
  float shoulder_gain;/* 0..1 (0=brak, 0.3=łagodnie), dodatkowe tłumienie w okolicy shoulder */
} ThrCurve_t;

typedef struct {
  ThrTrim_t  left;
  ThrTrim_t  right;
  ThrCurve_t curve;
} Throttle_Params_t;

/* domyślne ustawienia – punkt startowy */
extern const Throttle_Params_t THROTTLE_DEFAULTS;

/* Inicjalizacja parametrów (kopiowane do wnętrza modułu) */
void   Throttle_Init(const Throttle_Params_t *p);

/* Zastosuj kompensację i krzywą; in/out: -100..100 (zaokrąglone do int8) */
int8_t Throttle_Apply(int8_t in_percent, ThrSide_t side);

#ifdef __cplusplus
}
#endif
#endif
