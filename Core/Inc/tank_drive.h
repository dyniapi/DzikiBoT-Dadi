#pragma once
/*
 * ============================================================================
 *  MODULE: tank_drive — napęd 2×ESC (Left/Right) z rampą i reverse-gate
 *  ----------------------------------------------------------------------------
 *  CO:
 *    - Sterowanie dwoma ESC w układzie „czołgowym” (lewy/prawy tor).
 *    - Ograniczenie kroku zmian (rampa), wygładzanie EMA, bramka neutralu przy zmianie kierunku,
 *      mapowanie do „okna użytecznego” ESC (esc_start_pct → esc_max_pct).
 *
 *  PO CO:
 *    - Stabilny, przewidywalny moment od małych prędkości (minisumo) bez „szarpnięć”
 *      i bez ryzyka natychmiastowego reverse.
 *
 *  KIEDY:
 *    - Tank_Init()   — po uruchomieniu TIM1/ESC (np. w App_Init po ESC_ArmNeutral).
 *    - Tank_Update() — cyklicznie co CFG_Motors()->tick_ms (np. 20 ms ⇒ 50 Hz).
 *    - Pozostałe API — w reakcji na komendy wyższego poziomu (Stop/Forward/Rotate/SetTarget).
 *
 *  USTALENIA:
 *    - Kanały: TIM1_CH1 = PA8 → Right, TIM1_CH4 = PA11 → Left (mapowanie w warstwie ESC).
 *    - Skala mocy dla funkcji przyjmujących „%”: 0..100 (znak w SetTarget: −100..+100).
 * ============================================================================

 *
 *  Typowe zakresy strojenia (quick ref)
 *
 *  CFG_Motors()->tick_ms            : 10..50 ms   (typ. 20)     ↓mniej = szybciej reaguje; ↑więcej = lżej dla CPU
 *  CFG_Motors()->ramp_step_pct      : 1..10 %/tick (typ. 4)      ↑więcej = ostrzejszy start/hamowanie; ↓mniej = bardziej miękko
 *  CFG_Motors()->smooth_alpha       : 0.10..0.40  (typ. 0.25)    ↑więcej→mniej filtruje (bardziej „żywo”); ↓mniej→bardziej stabilnie
 *  CFG_Motors()->neutral_dwell_ms   : 200..800 ms (typ. 600)     ↑więcej = bezpieczniej przy reverse, wolniej zmienia kierunek
 *  CFG_Motors()->reverse_threshold_pct : 1..5 %   (typ. 3)       ↑więcej = trudniej „przeskoczyć” przez 0%; ↓mniej = ryzyko oscylacji
 *  CFG_Motors()->left_scale         : 0.90..1.10  (typ. 1.00)    korekta prostoliniowości (lewy tor)
 *  CFG_Motors()->right_scale        : 0.90..1.10  (typ. 1.00)    korekta prostoliniowości (prawy tor)
 *  CFG_Motors()->esc_start_pct      : 20..40 %    (typ. 30)      ↑więcej = mocniejszy „ciąg od dołu”, łatwiejsze ruszanie
 *  CFG_Motors()->esc_max_pct        : 50..80 %    (typ. 60)      ↓mniej = ograniczenie szczytowej mocy (kontrola trakcji)
 *
 *  Wskazówki:
 *    • „Zrywny start” minisumo: ramp_step_pct 5–8, smooth_alpha 0.20–0.30, esc_start_pct 30–35.
 *    • „Płynne manewry precyzyjne”: ramp_step_pct 2–4, smooth_alpha 0.25–0.35, esc_max_pct 55–65.
 *    • Jeśli ściąga na prostej: zmieniaj left/right_scale o 0.01 (1%) i testuj.
 * -------------------------------------------------------------------------- */




#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>                 // int8_t, uint*_t
#include "stm32l4xx_hal.h"          // TIM_HandleTypeDef (uchwyt timera z CubeMX)

/* ----------------------------------------------------------------------------
 *  Inicjalizacja napędu — zapamiętuje uchwyt do TIM1 i zeruje stan modułu.
 *  htim1: wskaźnik do uchwytu TIM1 skonfigurowanego do RC PWM (50 Hz).
 * ---------------------------------------------------------------------------- */
void Tank_Init(TIM_HandleTypeDef *htim1);

/* ----------------------------------------------------------------------------
 *  Aktualizacja napędu — wołaj co CFG_Motors()->tick_ms (np. w App_Tick()).
 *  Wewnątrz: reverse-gate → rampa → EMA → kompensacja torów → okno ESC → wyjście.
 * ---------------------------------------------------------------------------- */
void Tank_Update(void);

/* ----------------------------------------------------------------------------
 *  Wygodne manewry w skali 0..100% (bez znaków; znak implicit: forward/backward/rotate).
 *  Funkcje same przycinają zakres do 0..100.
 * ---------------------------------------------------------------------------- */
void Tank_Stop(void);                          // natychmiastowy cel: 0% / 0% (neutral)
void Tank_Forward(int8_t pct);                 // oba tory: +pct (0..100)
void Tank_Backward(int8_t pct);                // oba tory: -pct (0..100)
void Tank_TurnLeft(int8_t pct);                // łuk w lewo: lewe≈½, prawe=pct
void Tank_TurnRight(int8_t pct);               // łuk w prawo: lewe=pct, prawe≈½
void Tank_RotateLeft(int8_t pct);              // obrót w miejscu: lewe -pct, prawe +pct
void Tank_RotateRight(int8_t pct);             // obrót w miejscu: lewe +pct, prawe -pct

/* ----------------------------------------------------------------------------
 *  Ustaw cel bezpośrednio dla lewego i prawego toru (−100..0..+100).
 *  Biblioteka przytnie wartości do zakresu i zastosuje rampę/EMA/reverse-gate.
 * ---------------------------------------------------------------------------- */
void Tank_SetTarget(int8_t left_pct, int8_t right_pct);

#ifdef __cplusplus
}
#endif
