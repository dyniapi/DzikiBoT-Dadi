/**
 * @file    tank_drive.c
 * @brief   Napęd gąsienicowy 2×ESC (Left/Right) z rampą, bramką neutralu i mapowaniem do okna ESC.
 * @date    2025-11-02
 *
 * CO:
 *   - Sterowanie dwoma ESC (lewy/prawy) w logice „tank drive”.
 *   - Rampa (ograniczenie kroku zmian), wygładzanie EMA, reverse-gate (neutral przy zmianie znaku).
 *   - Mapowanie komendy logicznej −100..0..+100 do „okna użytecznego” ESC (esc_start_pct → esc_max_pct).
 *
 * PO CO:
 *   - Uzyskać kontrolowany, przewidywalny moment od niskich prędkości (minisumo),
 *     unikając „szarpnięć” i przypadkowego natychmiastowego reverse (chroni ESC/mechanikę).
 *
 * KIEDY:
 *   - Tank_Init()   — wołany raz po starcie, po ESC_Init(&htim1) / ESC_ArmNeutral(...).
 *   - Tank_Update() — wołany rytmicznie co CFG_Motors()->tick_ms (np. 20 ms ⇒ 50 Hz).
 *   - Tank_*()      — funkcje wyższego poziomu (Start/Stop/Forward/Backward/Turn/Rotate/SetTarget).
 *
 * KLUCZOWE PARAMETRY (zob. ConfigMotors_t w config.c):
 *   - tick_ms               : rytm pętli (krótszy = szybsza reakcja, większy narzut CPU).
 *   - ramp_step_pct         : krok rampy [%/tick] (większy = żywiej, mniejszy = łagodniej).
 *   - smooth_alpha (0..1)   : wygładzanie EMA (większy = mniej wygładza, szybsza reakcja).
 *   - neutral_dwell_ms      : czas „twardego neutralu” przy zmianie kierunku (reverse-gate).
 *   - reverse_threshold_pct : szerokość „martwej strefy” wokół 0% do detekcji zmiany znaku.
 *   - left_scale/right_scale: kompensacja asymetrii torów (1.00 = brak).
 *   - esc_start_pct/max_pct : „okno użyteczne” ESC (nasz 0..100% → [start..max]).
 *
 * FUNKCJE (skrót):
 *   - clamp_i8(int v, int lo, int hi)
 *   - clampf(float v, float lo, float hi)
 *   - ramp_once(int8_t *cur, int8_t tgt, uint8_t step)
 *   - ema_step(float prev, float in, float alpha)
 *   - map_logic_to_esc_window(int8_t x)
 *   - apply_neutral_gate_one(int8_t cur, int8_t tgt, uint8_t *gate_active, uint32_t *gate_until)
 *   - Tank_Init(TIM_HandleTypeDef *htim1)
 *   - Tank_Update(void)
 *   - Tank_Stop/Forward/Backward/TurnLeft/TurnRight/RotateLeft/RotateRight
 *   - Tank_SetTarget(int8_t left_pct, int8_t right_pct)
 *
 *
 * ============================================================================
 *  FAQ STROJENIA (tank_drive) — szybkie odpowiedzi „co zmienić i dlaczego”
 *  ----------------------------------------------------------------------------
 *  1) Po co „neutral-gate” (bramka neutralu)?
 *     • Chroni ESC i mechanikę przy nagłej zmianie kierunku (np. +80% → −80%).
 *     • Wymusza krótki neutral (C->neutral_dwell_ms), dając ESC czas na wejście w reverse.
 *
 *  2) Co robi reverse_threshold_pct?
 *     • To martwa strefa wokół 0% (np. ±3%). Zmiana znaku TYLKO gdy przekroczysz tę „bramkę”.
 *     • Większa wartość → trudniej „przeskoczyć” przez 0% (stabilniej, ale wolniej reaguje).
 *
 *  3) Rampa vs. smooth_alpha — jaka kolejność i efekt?
 *     • Najpierw rampa (ogranicza skok na tick), potem EMA (wygładza to, co już zrampowano).
 *     • ramp_step_pct kontroluje „twardość” fizycznej zmiany, smooth_alpha „miękkość” odbioru.
 *
 *  4) Co się dzieje, gdy gate_*_active = 1?
 *     • Kanał trzymany na 0% (neutral) do czasu gate_*_until (now + neutral_dwell_ms).
 *     • Rampa nie jest wykonywana (twardo 0%), by uniknąć „ciągnięcia” w trakcie reverse.
 *
 *  5) Po co okno ESC (esc_start_pct..esc_max_pct)?
 *     • Większość ESC ma martwą strefę i bardzo „czułą” górę. Okno daje przewidywalny dół
 *       (łatwy moment od małych wartości) i limituje szczyt (kontrola trakcji w minisumo).
 *
 *  6) Czemu EMA po rampie?
 *     • Gdyby EMA była przed rampą, duże skoki nadal przechodziłyby do rampy jako „twarde kroki”.
 *       Obecny porządek: najpierw ogranicz krok, potem wygładź to, co zostanie.
 *
 *  7) Kiedy zwiększyć left_scale/right_scale powyżej 1.0?
 *     • Gdy na prostej „ściąga” — np. left_scale=1.02 delikatnie podbije lewy tor. Zawsze w krokach 0.01.
 *
 *  8) Czy HAL_GetTick overflow jest bezpieczny?
 *     • Tak. Różnicowanie (now - tX) na typach bez znaku jest bezpieczne (wrap-around poprawnie zadziała).
 *
 *  9) Co jeśli tick (CFG_Motors()->tick_ms) ma jitter?
 *     • Ten kod porównuje różnicę czasu, nie wymaga idealnego interwału. Jitter w rozsądnych granicach jest OK.
 *
 *  10) Koło kręci się „odwrotnie” (pomyłka przewodów/mappingu)?
 *     • Zamień przewody silnika (dla BLDC) LUB odwróć znaczenie kanałów na poziomie ESC (nie tu).
 *       Warstwa tank_drive zakłada: dodatnie = „do przodu”.
 * ============================================================================ */


#include "tank_drive.h"     // deklaracje API tank drive (spójne z projektem)
#include "motor_bldc.h"     // wyjście do warstwy ESC (ESC_WritePercentRaw, ESC_SetNeutralAll)
#include "config.h"         // dostęp do CFG_Motors() — parametry rampy/okna/EMA itp.
#include "stm32l4xx_hal.h"  // HAL_GetTick() — zegar systemowy (ms)

#include <string.h>         // memset()

/* ============================================================================
 *                           STAN WEWNĘTRZNY MODUŁU
 * ==========================================================================*/
typedef struct {
    int8_t tgt_L, tgt_R;   /* target: żądane wartości użytkownika (−100..+100)           */
    int8_t cur_L, cur_R;   /* current: po rampie (−100..+100) — ograniczamy krok zmian  */
    float  flt_L, flt_R;   /* filtered: po wygładzeniu EMA (float)                       */
} TD_State_t;

static TD_State_t s = {0};            /* stan bieżący napędu (lewy/prawy)                 */
static TIM_HandleTypeDef *s_tim1 = NULL;      /* uchwyt TIM1 przekazany w Tank_Init()       */
static const ConfigMotors_t *C = NULL;        /* skrót do konfiguracji CFG_Motors()         */

/* Bramki neutralu (neutral-dwell) – osobno dla lewego i prawego koła.
 * Gdy wykryjemy istotną zmianę znaku (poniżej/ponad reverse_threshold_pct),
 * wymuszamy neutral (0%) przez neutral_dwell_ms. Dzięki temu ESC bezpiecznie
 * przechodzi w reverse, a mechanika nie dostaje „szarpnięcia”. */
static uint8_t  gate_L_active = 0, gate_R_active = 0;  /* 1=bramka aktywna, trzymaj neutral  */
static uint32_t gate_L_until  = 0, gate_R_until  = 0;  /* czas (ms), do którego bramka trwa  */

/* ============================================================================
 *                                 POMOCNICZE
 * ==========================================================================*/

/* clamp_i8: ogranicza wartość całkowitą do przedziału [lo..hi], zwraca int8_t */
static inline int8_t clamp_i8(int v, int lo, int hi)
{
    if (v < lo) v = lo;                 /* jeżeli mniej niż dolna granica — podnieś */
    if (v > hi) v = hi;                 /* jeżeli więcej niż górna granica — zetnij */
    return (int8_t)v;                   /* zwróć jako int8_t (−128..+127)          */
}

/* clampf: ogranicza float do [lo..hi] */
static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) v = lo;                 /* zabezpieczenie przed niedozwolonym min   */
    if (v > hi) v = hi;                 /* zabezpieczenie przed niedozwolonym max   */
    return v;                           /* zwrot wartości przyciętej                */
}

/* ramp_once: wykonuje P O J E D Y N C Z Y krok rampy z cur → tgt o max |step|.
 * Dzięki temu zmiany są „miękkie” (bez skoków), co odciąża mechanicę i ESC. */
static void ramp_once(int8_t *cur, int8_t tgt, uint8_t step)
{
    int d = (int)tgt - (int)*cur;       /* różnica: ile brakuje do celu             */
    if (d >  (int)step)  d =  (int)step;/* ogranicz: nie przekraczaj dodatniego kroku */
    else if (d < -(int)step) d = -(int)step; /* ogranicz: nie przekraczaj ujemnego kroku */
    *cur = (int8_t)((int)*cur + d);     /* zastosuj krok rampy (max ±step per tick) */
}

/* ema_step: pojedynczy krok wygładzania EMA (Exponential Moving Average).
 * alpha=0 → pełny filtr (brak zmian), alpha=1 → brak filtracji (natychmiast). */
static float ema_step(float prev, float in, float alpha)
{
    return (1.0f - alpha) * prev + alpha * in;  /* klasyczny wzór EMA             */
}

/* map_logic_to_esc_window:
 *  - wejście: komenda w skali logicznej −100..0..+100,
 *  - wyjście: „surowy” % wokół neutralu dla ESC, ale zawężony do [start..max],
 *  - 0 → neutral (0%), dodatnie → powyżej neutralu, ujemne → poniżej.
 *  - docelowo warstwa ESC przemapuje % liniowo na 1000..2000 µs (1..2 ms). */
static int8_t map_logic_to_esc_window(int8_t x)
{
    const uint8_t start = C->esc_start_pct;   /* np. 30% — wyjście z martwej strefy        */
    const uint8_t max   = C->esc_max_pct;     /* np. 60% — nasz „sufit” dla 100% logicznego */

    if (x == 0) return 0;                     /* 0 logiczne = neutral (1500 µs)            */

    const int8_t sign = (x < 0) ? -1 : +1;    /* znak komendy — określa kierunek            */
    int          mag  = (x < 0) ? -x : x;     /* moduł (0..100)                             */

    /* Liniowy udział w oknie [start..max], z kontrolą brzegów. */
    int esc_pct = (int)start + ((int)(max - start) * mag) / 100;
    if (esc_pct < (int)start) esc_pct = (int)start;   /* zabezpieczenie dolnej krawędzi */
    if (esc_pct > (int)max)   esc_pct = (int)max;     /* zabezpieczenie górnej krawędzi */

    return (int8_t)(sign * esc_pct);          /* dodaj znak: +powyżej / −poniżej neutralu  */
}

/* apply_neutral_gate_one:
 *  - jeżeli bramka aktywna (gate_active=1) i czas nie minął → trzymaj neutral (0%),
 *  - jeżeli wykryto zmianę znaku (ponad próg reverse_threshold_pct) → włącz bramkę,
 *    ustaw czas wygaśnięcia i natychmiast zwróć neutral (0%),
 *  - w przeciwnym wypadku zwróć docelową wartość 'tgt'. */
static int8_t apply_neutral_gate_one(int8_t cur, int8_t tgt,
                                     uint8_t *gate_active, uint32_t *gate_until)
{
    const uint32_t now      = HAL_GetTick();           /* bieżący czas [ms]             */
    const uint16_t dwell_ms = C->neutral_dwell_ms;     /* czas trwania bramki           */
    const int8_t   thr      = (int8_t)C->reverse_threshold_pct; /* próg zmiany znaku  */

    if (*gate_active) {                                /* jeśli bramka już trwa...      */
        if ((int32_t)(now - *gate_until) >= 0) {       /* ...i czas minął → wyłącz      */
            *gate_active = 0;
        } else {
            return 0;                                  /* ...jeszcze trwa → trzymaj 0%  */
        }
    }

    /* Detekcja „istotnej” zmiany znaku (poza martwą strefą ±thr) */
    if ((cur > +thr && tgt < -thr) || (cur < -thr && tgt > +thr)) {
        *gate_active = 1;                              /* aktywuj bramkę neutralu       */
        *gate_until  = now + dwell_ms;                 /* zapamiętaj kiedy wygasić      */
        return 0;                                      /* od razu neutral               */
    }

    return tgt;                                        /* bez zmian — jedź do celu      */
}

/* ============================================================================
 *                                     API
 * ==========================================================================*/

/* Tank_Init:
 *  - resetuje stan sterownika, zapamiętuje uchwyt TIM1, pobiera wskaźnik do konfiguracji,
 *  - ustawia bezpieczny neutral na start. */
void Tank_Init(TIM_HandleTypeDef *htim1)
{
    memset(&s, 0, sizeof(s));          /* wyzeruj cele, bieżące i filtry (bez śmieci) */
    s_tim1 = htim1;                    /* zapamiętaj uchwyt TIM1 (kanały CH1/CH4)    */
    C = CFG_Motors();                  /* pobierz konfigurację napędu                */

    gate_L_active = gate_R_active = 0; /* brak aktywnych bramek na starcie           */
    gate_L_until  = gate_R_until  = 0; /* czasy wygaszenia = 0                       */

    ESC_SetNeutralAll();               /* obie strony 1500 µs — bezpieczny start     */
}

/* Tank_Update:
 *  - wywoływać periodycznie co C->tick_ms (np. co 20 ms),
 *  - kolejność: neutral-gate → rampa → EMA → kompensacja L/R → okno ESC → wyjście. */
void Tank_Update(void)
{
    if (!C) {                          /* zabezpieczenie: jeżeli ktoś wołał przed Init */
        C = CFG_Motors();              /* dociągnij konfigurację, by nie dereferencjon. */
    }

    /* 0) Neutral-dwell — docelowa wartość po uwzględnieniu ewent. bramki neutralu */
    const int8_t gated_tgt_L = apply_neutral_gate_one(s.cur_L, s.tgt_L,
                                                      &gate_L_active, &gate_L_until);
    const int8_t gated_tgt_R = apply_neutral_gate_one(s.cur_R, s.tgt_R,
                                                      &gate_R_active, &gate_R_until);

    /* 1) Rampa — pojedynczy krok cur→tgt (lub neutral, jeśli gate aktywna) */
    if (gate_L_active) s.cur_L = 0;    /* gdy gate → twardy neutral bez rampy       */
    else               ramp_once(&s.cur_L, gated_tgt_L, C->ramp_step_pct);

    if (gate_R_active) s.cur_R = 0;    /* jw. dla prawego koła                      */
    else               ramp_once(&s.cur_R, gated_tgt_R, C->ramp_step_pct);

    /* 2) Wygładzanie (EMA) — redukuje drobne oscylacje, czyni sterowanie „miękkim” */
    const float inL = (float)s.cur_L;  /* rzutowanie na float do filtra             */
    const float inR = (float)s.cur_R;
    if (C->smooth_alpha > 0.0f) {      /* 0.0 = wyłączony filtr EMA                 */
        s.flt_L = ema_step(s.flt_L, inL, C->smooth_alpha);
        s.flt_R = ema_step(s.flt_R, inR, C->smooth_alpha);
    } else {
        s.flt_L = inL;                 /* bez filtrowania → przepisz wartość        */
        s.flt_R = inR;
    }

    /* 3) Kompensacja torów — mnożymy lewy/prawy przez left/right_scale i tniemy do ±100% */
    const float compL = clampf(s.flt_L * C->left_scale,  -100.0f, 100.0f);
    const float compR = clampf(s.flt_R * C->right_scale, -100.0f, 100.0f);

    /* 4) Mapowanie do okna ESC i wyjście do warstwy PWM (Left→CH4, Right→CH1) */
    const int8_t outL_raw = map_logic_to_esc_window((int8_t)compL); /* −100..+100 (wokół 0) */
    const int8_t outR_raw = map_logic_to_esc_window((int8_t)compR);

    ESC_WritePercentRaw(ESC_CH4, outL_raw);  /* Left  – TIM1_CH4 (PA11)  → RC 1..2 ms */
    ESC_WritePercentRaw(ESC_CH1, outR_raw);  /* Right – TIM1_CH1 (PA8)   → RC 1..2 ms */
}

/* ==== API wysokiego poziomu – proste manewry w skali 0..100% ==== */

void Tank_Stop(void)
{
    s.tgt_L = 0;                        /* zatrzymaj lewe koło  (0% = neutral)       */
    s.tgt_R = 0;                        /* zatrzymaj prawe koło (0% = neutral)       */
}

void Tank_Forward(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);        /* upewnij się, że 0..100                    */
    s.tgt_L = +pct;                     /* oba koła „w przód”                        */
    s.tgt_R = +pct;
}

void Tank_Backward(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);        /* upewnij się, że 0..100                    */
    s.tgt_L = -pct;                     /* oba koła „w tył”                          */
    s.tgt_R = -pct;
}

/* arc_pair: pomocniczo — skręt po łuku (wewnętrzne ≈ 50% zewnętrznego).
 * W razie potrzeby można zrobić z tego parametr w config (np. 40..70%). */
static void arc_pair(int8_t base, int8_t *inner, int8_t *outer)
{
    int in  = (int)base / 2;            /* wewnętrzne ≈ połowa                       */
    int out = (int)base;                 /* zewnętrzne = cała wartość                 */
    if (in  < 0) in  = 0;               /* nie dopuszczamy wartości ujemnych tutaj   */
    if (out < 0) out = 0;
    *inner = (int8_t)in;                 /* ustaw wynik dla koła wewnętrznego         */
    *outer = (int8_t)out;                /* ustaw wynik dla koła zewnętrznego         */
}

void Tank_TurnLeft(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);        /* 0..100, skręt w lewo po łuku              */
    int8_t in, out;
    arc_pair(pct, &in, &out);
    s.tgt_L = +in;                       /* lewe = wewnętrzne                         */
    s.tgt_R = +out;                      /* prawe = zewnętrzne                        */
}

void Tank_TurnRight(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);        /* 0..100, skręt w prawo po łuku             */
    int8_t in, out;
    arc_pair(pct, &in, &out);
    s.tgt_L = +out;                      /* lewe  = zewnętrzne                        */
    s.tgt_R = +in;                       /* prawe = wewnętrzne                        */
}

void Tank_RotateLeft(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);        /* 0..100, obrót w miejscu w lewo            */
    s.tgt_L = -pct;                      /* lewe  wstecz                               */
    s.tgt_R = +pct;                      /* prawe w przód                              */
}

void Tank_RotateRight(int8_t pct)
{
    pct = clamp_i8(pct, 0, 100);        /* 0..100, obrót w miejscu w prawo           */
    s.tgt_L = +pct;                      /* lewe  w przód                              */
    s.tgt_R = -pct;                      /* prawe wstecz                               */
}

void Tank_SetTarget(int8_t left_pct, int8_t right_pct)
{
    s.tgt_L = clamp_i8(left_pct,  -100, 100);  /* bezpośrednie cele (−100..+100)       */
    s.tgt_R = clamp_i8(right_pct, -100, 100);
}
