/*
 * tf_luna_i2c.h
 *
 *  Created on: Oct 22, 2025
 *      Author: DzikiBoT
 */

/**
 ******************************************************************************
 * @file    tf_luna_i2c.h
 * @brief   TF-Luna over I2C – odporna obsługa (burst + fallback rejestrami)
 *          + filtry (MED5 dla distance, MA5 dla strength)
 * @version 3.0
 * @date    26.10.2025
 *
 *  Adres I2C: 0x10 (7-bit) → HAL (8-bit) = 0x20.
 *
 *  Zwracane jednostki:
 *    - distance          [cm]      (uint16_t, surowe)
 *    - distance_filt     [cm]      (uint16_t, MED5)
 *    - strength          [raw]     (uint16_t, surowe)
 *    - strength_filt     [raw]     (uint16_t, MA5)
 *    - temperature       [°C]      (float, już w stopniach C)
 *    - frameReady        1/0       (czy świeża ramka poprawna)
 *
 *  API rozdzielone na Right (I2C1) i Left (I2C3).
 ******************************************************************************
 */

#ifndef TF_LUNA_I2C_H_
#define TF_LUNA_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* Dane pomiarowe TF-Luna (surowe + filtrowane) */
typedef struct {
    uint16_t distance;        /* [cm]  surowe */
    uint16_t distance_filt;   /* [cm]  MED5  */
    uint16_t strength;        /* [raw] surowe */
    uint16_t strength_filt;   /* [raw] MA5   */
    float    temperature;     /* [°C]  (float, już w °C) */
    uint8_t  frameReady;      /* 1 = nowa poprawna ramka, 0 = brak/błąd */
} TF_LunaData_t;

/* Inicjalizacja uchwytów I2C dla prawego (I2C1) i lewego (I2C3) czujnika */
void          TF_Luna_Right_Init(I2C_HandleTypeDef *hi2c1);
void          TF_Luna_Left_Init (I2C_HandleTypeDef *hi2c3);

/* Odczyt pojedynczej ramki (odporne: burst 9B → fallback rejestrami), z filtrami */
TF_LunaData_t TF_Luna_Right_Read(void);
TF_LunaData_t TF_Luna_Left_Read (void);

/* (Opcjonalnie) reset filtrów – jeśli potrzebujesz w kodzie serwisowym */
void          TF_Luna_Right_ResetFilters(void);
void          TF_Luna_Left_ResetFilters(void);

#ifdef __cplusplus
}
#endif
#endif /* TF_LUNA_I2C_H_ */
