/**
 ******************************************************************************
 * @file    tank_drive.c
 * @brief   „Tank drive”: rampa, EMA, kompensacja L/R, mapowanie okna ESC,
 *          neutral-dwell przy zmianie kierunku (crawler ESC), wyjście do ESC.
 * @date    2025-10-28
 *
 * Architektura:
 *  - Całe strojenie w config.[ch] (blok [MOTORS]):
 *      * tick_ms              – okres wywołań Tank_Update()
 *      * ramp_step_pct        – krok rampy (w % na tick)
 *      * smooth_alpha         – wygładzanie EMA (0.0 wyłącza)
 *      * left_scale/right_scale – kompensacja asymetrii kół
 *      * esc_start_pct/esc_max_pct – okno pracy ESC (np. 30..60 %)
 *      * neutral_dwell_ms     – czas twardego neutralu przy zmianie kierunku
 *      * reverse_threshold_pct – próg uznania „istotnej” zmiany znaku
 *
 *  - API wysokiego poziomu:
 *      Tank_Forward/Backward/TurnLeft/TurnRight/RotateLeft/RotateRight/Stop
 *    oraz Tank_SetTarget(L,R) (−100..+100).
 *
 *  - Wyjście do ESC zawsze przez motor_bldc (liniowo 1..2 ms).
 ******************************************************************************
 */

#include "tank_drive.h"
#include "motor_bldc.h"
#include "config.h"

#include <string.h>

/* ============================================================================
 *                           STAN WEWNĘTRZNY MODUŁU
 * ==========================================================================*/
typedef struct {
    int8_t tgt_L, tgt_R;   /* cele użytkownika (−100..100) */
    int8_t cur_L, cur_R;   /* po rampie (−100..100)        */
    float  flt_L, flt_R;   /* po wygładzeniu (float)       */
} TD_State_t;

static TD_State_t s = {0};
static TIM_HandleTypeDef *s_tim1 = NULL;
static const ConfigMotors_t *C = NULL;

/* Bramki neutralu (neutral-dwell) – per koło.
   Gdy wykryjemy zmianę znaku prędkości (powyżej progu), wymuszamy 0% przez
   C->neutral_dwell_ms, aby crawler-ESC poprawnie przeszło w reverse. */
static uint8_t  gate_L_active = 0, gate_R_active = 0;
static uint32_t gate_L_until  = 0, gate_R_until  = 0;

/* ============================================================================
 *                                 POMOCNICZE
 * ==========================================================================*/
static inline int8_t clamp_i8(int v, int lo, int hi)
{
    if (v < lo) {
        v = lo;
    }
    if (v > hi) {
        v = hi;
    }
    return (int8_t)v;
}

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        v = lo;
    }
    if (v > hi) {
        v = hi;
    }
    return v;
}

/* Rampa: pojedynczy krok cur → tgt, z ograniczeniem o krok „step”. */
static void ramp_once(int8_t *cur, int8_t tgt, uint8_t step)
{
    int d = (int)tgt - (int)*cur;
    if (d >  (int)step) {
        d =  (int)step;
    } else if (d < -(int)step) {
        d = -(int)step;
    }
    *cur = (int8_t)((int)*cur + d);
}

/* EMA (Exponential Moving Average) – pojedynczy krok wygładzania. */
static float ema_step(float prev, float in, float alpha)
{
    return (1.0f - alpha) * prev + alpha * in;
}

/* Mapowanie komendy logicznej (−100..+100) do „surowego” %-outputu
   wokół neutralu dla ESC (−100..0..+100), ale z zawężeniem do okna
   [esc_start_pct .. esc_max_pct] dla |x|>0. x==0 → 0 (neutral).
   Surowy % → motor_bldc przemapowuje liniowo na 1..2 ms. */
static int8_t map_logic_to_esc_window(int8_t x)
{
    const uint8_t start = C->esc_start_pct; /* np. 30 */
    const uint8_t max   = C->esc_max_pct;   /* np. 60 */

    if (x == 0) {
        return 0; /* neutral */
    }

    const int8_t sign = (x < 0) ? -1 : +1;
    int          mag  = (x < 0) ? -x : x;  /* 0..100 */

    /* Wyznacz poziom w [start..max] */
    int esc_pct = (int)start + ((int)(max - start) * mag) / 100;
    if (esc_pct < (int)start) {
        esc_pct = (int)start;
    }
    if (esc_pct > (int)max) {
        esc_pct = (int)max;
    }

    /* Znak przenosi kierunek: dodatni → powyżej neutralu, ujemny → poniżej. */
    return (int8_t)(sign * esc_pct);
}

/* Neutral-dwell (bramka neutralu) dla jednego koła:
   * jeśli aktywna – trzymaj 0% aż do upłynięcia gate_until,
   * jeśli wykryto istotną zmianę znaku (cur→tgt) – aktywuj bramkę. */
static int8_t apply_neutral_gate_one(int8_t cur, int8_t tgt,
                                     uint8_t *gate_active, uint32_t *gate_until)
{
    const uint32_t now = HAL_GetTick();
    const uint16_t dwell_ms = C->neutral_dwell_ms;
    const int8_t   thr      = (int8_t)C->reverse_threshold_pct;

    if (*gate_active) {
        if ((int32_t)(now - *gate_until) >= 0) {
            *gate_active = 0;      /* koniec „pauzy” neutralu */
        } else {
            return 0;              /* trzymaj twardy neutral */
        }
    }

    /* Zmiana znaku powyżej progu? Włącz pauzę neutralu. */
    if ((cur > +thr && tgt < -thr) ||
        (cur < -thr && tgt > +thr))
    {
        *gate_active = 1;
        *gate_until  = now + dwell_ms;
        return 0;                  /* natychmiast neutral */
    }

    return tgt;
}

/* ============================================================================
 *                                     API
 * ==========================================================================*/
void Tank_Init(TIM_HandleTypeDef *htim1)
{
    memset(&s, 0, sizeof(s));
    s_tim1 = htim1;
    C = CFG_Motors();

    gate_L_active = gate_R_active = 0;
    gate_L_until  = gate_R_until  = 0;

    /* Bezpieczny stan na starcie */
    ESC_SetNeutralAll();
}

/* Główna aktualizacja — wywołuj co CFG_Motors()->tick_ms (np. 20 ms).
   Kolejność:
     0) neutral-dwell (twardy neutral przy zmianie kierunku),
     1) rampa cur→tgt,
     2) EMA,
     3) kompensacja L/R,
     4) mapowanie do okna ESC i wyjście na ESC. */


void Tank_Update(void)
{
    if (!C) {
        C = CFG_Motors();
    }

    /* 0) Neutral-dwell: ewentualne wymuszenie twardego neutralu. */
    const int8_t gated_tgt_L = apply_neutral_gate_one(s.cur_L, s.tgt_L,
                                                      &gate_L_active, &gate_L_until);
    const int8_t gated_tgt_R = apply_neutral_gate_one(s.cur_R, s.tgt_R,
                                                      &gate_R_active, &gate_R_until);

    /* 1) Rampa (jeśli gate aktywna – trzymamy 0 bez rampy) */
    if (gate_L_active) {
        s.cur_L = 0;
    } else {
        ramp_once(&s.cur_L, gated_tgt_L, C->ramp_step_pct);
    }

    if (gate_R_active) {
        s.cur_R = 0;
    } else {
        ramp_once(&s.cur_R, gated_tgt_R, C->ramp_step_pct);
    }

    /* 2) Wygładzanie (EMA) */
    const float inL = (float)s.cur_L;
    const float inR = (float)s.cur_R;
    if (C->smooth_alpha > 0.0f) {
        s.flt_L = ema_step(s.flt_L, inL, C->smooth_alpha);
        s.flt_R = ema_step(s.flt_R, inR, C->smooth_alpha);
    } else {
        s.flt_L = inL;
        s.flt_R = inR;
    }

    /* 3) Kompensacja L/R */
    const float compL = clampf(s.flt_L * C->left_scale,  -100.0f, 100.0f);
    const float compR = clampf(s.flt_R * C->right_scale, -100.0f, 100.0f);

    /* 4) Mapowanie do okna ESC i wyjście (Left→CH4, Right→CH1) */
    const int8_t outL_raw = map_logic_to_esc_window((int8_t)compL);
    const int8_t outR_raw = map_logic_to_esc_window((int8_t)compR);

    ESC_WritePercentRaw(ESC_CH4, outL_raw);  /* Left  – TIM1_CH4 (PA11) */
    ESC_WritePercentRaw(ESC_CH1, outR_raw);  /* Right – TIM1_CH1 (PA8)  */
}

/* ==== API wysokiego poziomu – manewry podstawowe ==== */
void Tank_Stop(void)
{
    s.tgt_L = 0;
    s.tgt_R = 0;
}

void Tank_Forward(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);
    s.tgt_L = +pct;
    s.tgt_R = +pct;
}

void Tank_Backward(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);
    s.tgt_L = -pct;
    s.tgt_R = -pct;
}

/* „Skręt” po łuku: koło wewnętrzne ~50% wartości, zewnętrzne 100%.
   Ten współczynnik łatwo przenieść do config, jeśli chcesz go stroić. */
static void arc_pair(int8_t base, int8_t *inner, int8_t *outer)
{
    int in  = (int)base / 2;   /* 50% */
    int out = (int)base;
    if (in  < 0) { in  = 0;  }
    if (out < 0) { out = 0;  }
    *inner = (int8_t)in;
    *outer = (int8_t)out;
}

void Tank_TurnLeft(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);
    int8_t in, out;
    arc_pair(pct, &in, &out);
    s.tgt_L = +in;    /* lewe = wewnętrzne */
    s.tgt_R = +out;   /* prawe = zewnętrzne */
}

void Tank_TurnRight(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);
    int8_t in, out;
    arc_pair(pct, &in, &out);
    s.tgt_L = +out;   /* lewe  = zewnętrzne */
    s.tgt_R = +in;    /* prawe = wewnętrzne */
}

void Tank_RotateLeft(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);
    s.tgt_L = -pct;
    s.tgt_R = +pct;
}

void Tank_RotateRight(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);
    s.tgt_L = +pct;
    s.tgt_R = -pct;
}

void Tank_SetTarget(int8_t left_pct, int8_t right_pct)
{
    s.tgt_L = clamp_i8(left_pct,  -100, 100);
    s.tgt_R = clamp_i8(right_pct, -100, 100);
}
