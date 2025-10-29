/**
 ******************************************************************************
 * @file    tank_test.h
 * @brief   Nieblokujący test jazdy DzikiBoT na bazie tank_drive:
 *          3s przód → 2s skręt w lewo → 2s skręt w prawo → stop → obrót 180° → stop.
 * @date    2025-10-26
 ******************************************************************************
 */
#ifndef TANK_TEST_H_
#define TANK_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief Start testu (nieblokująco).
 * @param fwd_speed       prędkość do przodu   (-100..100), np. 60
 * @param turn_speed      prędkość skrętu      (  0..100),  np. 60  (używana jako |L|=S, |R|=S z przeciwnym znakiem)
 * @param fwd_time_ms     czas jazdy prosto [ms], np. 3000
 * @param left_time_ms    czas skrętu w lewo [ms], np. 2000
 * @param right_time_ms   czas skrętu w prawo [ms], np. 2000
 * @param spin180_time_ms czas obrotu o ~180° [ms], np. 1200..1800 (dobierzesz do robota)
 */
void TankTest_Start(int8_t fwd_speed,
                    uint8_t turn_speed,
                    uint32_t fwd_time_ms,
                    uint32_t left_time_ms,
                    uint32_t right_time_ms,
                    uint32_t spin180_time_ms);

/** Jedno „tyknięcie” automatu – wołaj często (≥ co 10–20 ms). */
void TankTest_Tick(void);

/** Czy test nadal trwa? (1 = w toku, 0 = zakończony) */
uint8_t TankTest_IsRunning(void);

/** Wymuś natychmiastowy STOP i zakończ test. */
void TankTest_Abort(void);

#ifdef __cplusplus
}
#endif
#endif /* TANK_TEST_H_ */
