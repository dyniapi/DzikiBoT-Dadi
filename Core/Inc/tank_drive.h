/**
 * @file    tank_drive.h
 * @brief   „Tank drive” – rampa, EMA, kompensacja L/R, mapowanie do okna ESC
 * @date    2025-10-28
 *
 * Logika:
 *  - Rampa (miękkie zmiany) – krok %/tick z config
 *  - Wygładzanie EMA (opcjonalne) – współczynnik alpha w config
 *  - Kompensacja L/R (skalowanie) – z config
 *  - Mapowanie komendy logicznej [-100..100] do „okna ESC” [start..max] %,
 *    gdzie start=30%, max=60% (domyślnie) pobierane z config:
 *      |cmd|=0 → neutral (0%)
 *      |cmd|>0 → [start..max] i znak → kierunek
 *
 * Wystawianie na ESC robi motor_bldc (liniowo w 1..2 ms).
 */

#ifndef TANK_DRIVE_H
#define TANK_DRIVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "tim.h"
#include <stdint.h>

/* Inicjalizacja – podaj uchwyt TIM1 (po ESC_Init) */
void Tank_Init(TIM_HandleTypeDef *htim1);

/* Wywołuj co tick (np. 20 ms wg config) – realizuje rampę/EMA/kompensację i wyjścia */
void Tank_Update(void);

/* Proste API manewrów (wartości wejściowe 0..100; znak kierunku zawierają funkcje) */
void Tank_Stop(void);
void Tank_Forward(int8_t pct);      /* oba koła +pct   */
void Tank_Backward(int8_t pct);     /* oba koła -pct   */
void Tank_TurnLeft(int8_t pct);     /* łuk: L=~50%*pct, R=~100%*pct */
void Tank_TurnRight(int8_t pct);    /* łuk: L=~100%*pct, R=~50%*pct */
void Tank_RotateLeft(int8_t pct);   /* w miejscu: L=-pct, R=+pct */
void Tank_RotateRight(int8_t pct);  /* w miejscu: L=+pct, R=-pct */

/* Bezpośrednie ustawienie celu L/R (−100..+100) */
void Tank_SetTarget(int8_t left_pct, int8_t right_pct);

#ifdef __cplusplus
}
#endif
#endif /* TANK_DRIVE_H */
