/**
 * @file    tcs3472.h
 * @brief   Obsługa TCS3472 (I²C): odczyt RAW C/R/G/B, skalowania i progi.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 */

/*
 * tcs3472.h
 *
 *  Created on: Oct 22, 2025
 *      Author: dyniapi
 */


#ifndef TCS3472_H_
#define TCS3472_H_

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif




typedef struct {
    uint16_t clear;
    uint16_t red;
    uint16_t green;
    uint16_t blue;
} TCS3472_Data_t;

/* Right = I2C1, Left = I2C3 */
void           TCS3472_Right_Init(I2C_HandleTypeDef *hi2c1);
void           TCS3472_Left_Init (I2C_HandleTypeDef *hi2c3);
TCS3472_Data_t TCS3472_Right_Read(void);
TCS3472_Data_t TCS3472_Left_Read (void);

#ifdef __cplusplus
}
#endif
#endif /* TCS3472_H_ */
