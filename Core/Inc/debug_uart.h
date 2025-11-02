/**
 * @file    debug_uart.h
 * @brief   Panel UART 115200 8N1 (ANSI) do debugowania w miejscu.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 */

#ifndef DEBUG_UART_H_
#define DEBUG_UART_H_

#include "main.h"
#include <stdarg.h>   // << potrzebne dla ... (printf-like)

#ifdef __cplusplus
extern "C" {
#endif

void DebugUART_Init(UART_HandleTypeDef *huart);

/* Byłeś już z tego korzystał: jedna linia z CRLF */
void DebugUART_Print(const char *msg);

/* NOWE: formatowana wersja (zgodna z printf) z CRLF na końcu */
void DebugUART_Printf(const char *fmt, ...);

/* Panel 2-kolumnowy (bez zmian) */
#include "tf_luna_i2c.h"
#include "tcs3472.h"
void DebugUART_SensorsDual(
    const TF_LunaData_t *RightLuna, const TF_LunaData_t *LeftLuna,
    const TCS3472_Data_t *RightColor, const TCS3472_Data_t *LeftColor);

#ifdef __cplusplus
}
#endif
#endif /* DEBUG_UART_H_ */
