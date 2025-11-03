/**
 * @file    debug_uart.h
 * @brief   Debug panel UART (ANSI), TX nieblokujące (IRQ), kompatybilne API z core.zip.
 * @date    2025-11-03
 *
 * Co udostępnia:
 *   - DebugUART_Init()      : inicjalizacja z uchwytem HAL UART (np. &huart2).
 *   - DebugUART_Print()     : szybkie wysłanie stringa + CRLF (enqueue, bez blokowania).
 *   - DebugUART_Printf()    : printf -> enqueue + CRLF (bez blokowania).
 *   - DebugUART_SensorsDual(): dwukolumnowy panel (RIGHT I2C1 | LEFT I2C3).
 *   - DebugUART_Dropped()   : licznik bajtów utraconych przy przepełnieniu kolejki TX.
 *
 * Uwaga:
 *   - Wysyłanie wykonywane przez HAL_UART_Transmit_IT z wewnętrznego bufora kołowego.
 *   - Gdy kolejka się zapełni, nadmiar jest dyskretnie odrzucany (bez blokowania pętli).
 */

#ifndef DEBUG_UART_H_
#define DEBUG_UART_H_

#include "main.h"   // zawiera m.in. stm32l4xx_hal.h i definicje huart2

#ifdef __cplusplus
extern "C" {
#endif

/* Rozmiar bufora TX (bajtów). 1024 to bezpieczne minimum dla Twojego panelu. */
#ifndef DEBUG_UART_RB_SIZE
#define DEBUG_UART_RB_SIZE 1024u
#endif

/* =============================== API ================================== */

/* Inicjalizacja: zapamiętujemy uchwyt UART, czyścimy kolejkę TX. */
void DebugUART_Init(UART_HandleTypeDef *huart);

/* Wysłanie C-stringa (bez formatowania) + CRLF, nieblokujące (enqueue). */
void DebugUART_Print(const char *msg);

/* Wysłanie sformatowanego tekstu (printf) + CRLF, nieblokujące (enqueue). */
void DebugUART_Printf(const char *fmt, ...);

/* Panel dwukolumnowy — zgodny z baseline (działa na danych driverów). */
#include "tf_luna_i2c.h"   // TF_LunaData_t, TF_Luna_AmbientEstimateC()
#include "tcs3472.h"       // TCS3472_Data_t
void DebugUART_SensorsDual(
    const TF_LunaData_t *RightLuna, const TF_LunaData_t *LeftLuna,
    const TCS3472_Data_t *RightColor, const TCS3472_Data_t *LeftColor);

/* Getter liczby bajtów, których nie udało się wstawić do kolejki (przepełnienie). */
uint32_t DebugUART_Dropped(void);

#ifdef __cplusplus
}
#endif
#endif /* DEBUG_UART_H_ */
