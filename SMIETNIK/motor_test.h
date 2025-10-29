/**
 * @file    motor_test.h
 * @brief   Nieblokujący test ESC: 5s FWD -> STOP -> 5s REV -> STOP (z rampą)
 */
#ifndef MOTOR_TEST_H_
#define MOTOR_TEST_H_

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start testu.
 * ramp_rate: maks. zmiana prędkości na krok (jednostki: % na krok wywołania)
 * tick_ms  : krok czasowy automatu (np. 20 ms; wywołuj MotorTest_Tick() >= tak często)
 */
void MotorTest_Start(uint8_t ramp_rate, uint16_t tick_ms);

/* Jedno „tyknięcie” automatu testowego (wywołuj w pętli głównej, np. co 1..5 ms). */
void MotorTest_Tick(void);

/* Czy test jest aktywny (pracuje automat)? */
uint8_t MotorTest_IsRunning(void);

#ifdef __cplusplus
}
#endif
#endif /* MOTOR_TEST_H_ */
