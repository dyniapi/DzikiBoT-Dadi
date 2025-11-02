/**
 * @file    i2c_scan.h
 * @brief   Skanowanie urządzeń na I²C do weryfikacji podłączeń.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 */

/*
 * i2c_scan.h
 *
 *  Created on: Oct 24, 2025
 *      Author: dyniapi
 */



#ifndef I2C_SCAN_H_
#define I2C_SCAN_H_

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint8_t I2C_Scan_Bus(const char* busName,
                     I2C_HandleTypeDef* hi2c,
                     uint8_t start7b, uint8_t end7b,
                     uint8_t trials, uint32_t timeout);


void I2C_Scan_All(void);

#ifdef __cplusplus
}
#endif
#endif /* I2C_SCAN_H_ */
