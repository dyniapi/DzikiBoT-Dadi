/*
 * i2c_scan.h
 *
 *  Created on: Oct 24, 2025
 *      Author: dyniapi
 */

/**
 ******************************************************************************
 * @file    i2c_scan.h
 * @brief   Skaner magistral I2C (Right = I2C1, Left = I2C3)
 * @author  DzikiBoT
 * @version 1.0
 * @date    2025-10-24
 *
 * @details
 *  - Moduł skanuje zakres adresów 7-bit: 0x08..0x77
 *  - Wynik wypisuje na UART przez DebugUART_Printf()
 *  - Funkcja I2C_Scan_All() skanuje I2C1 i I2C3 + podaje podsumowanie
 *  - Opcjonalnie możesz użyć I2C_Scan_Bus() dla pojedynczej magistrali
 *
 *  Wymagania:
 *   - Zainicjalizowany UART + DebugUART_Init()
 *   - Zainicjalizowane I2C1 i I2C3
 ******************************************************************************
 */

#ifndef I2C_SCAN_H_
#define I2C_SCAN_H_

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Skanuje pojedynczą magistralę I2C i wypisuje znalezione adresy.
 * @param  busName  Przyjazna nazwa (np. "Right (I2C1)")
 * @param  hi2c     Uchwyt HAL do I2C (np. &hi2c1)
 * @param  start7b  Początkowy adres 7-bit (domyślnie 0x08)
 * @param  end7b    Końcowy adres 7-bit (domyślnie 0x77)
 * @param  trials   Liczba prób (np. 2)
 * @param  timeout  Timeout w ms (np. 2)
 * @return Liczba znalezionych urządzeń na tej magistrali
 */
uint8_t I2C_Scan_Bus(const char* busName,
                     I2C_HandleTypeDef* hi2c,
                     uint8_t start7b, uint8_t end7b,
                     uint8_t trials, uint32_t timeout);

/**
 * @brief  Skanuje obie magistrale: Right (I2C1) i Left (I2C3).
 *         Wypisuje czytelną tabelę w terminalu.
 */
void I2C_Scan_All(void);

#ifdef __cplusplus
}
#endif
#endif /* I2C_SCAN_H_ */
