#pragma once
/**
 * ============================================================================
 *  MODULE: tf_luna_i2c — minimalna obsługa TF-Luna (I²C)
 *  ----------------------------------------------------------------------------
 *  CO:
 *    • Odczyt rejestrów 0x00..0x05: distance [cm], strength [raw], temperature [0.01°C].
 *    • Filtry: mediana (distance) + średnia krocząca (strength) wg okien z config.c.
 *    • Temperatura zwracana jako °C (float) z dokładnością 0.1°C.
 *
 *  PO CO:
 *    • Stabilny, prosty i czytelny odczyt — bez nieużywanych ścieżek (burst/CRC/auto-detect).
 *
 *  KIEDY:
 *    • *_Init() wywołaj raz po starcie.
 *    • *_Read() wywołuj cyklicznie (np. co CFG_Scheduler()->sens_ms).
 * ============================================================================
 */

#ifndef TF_LUNA_I2C_H_
#define TF_LUNA_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"     // I2C_HandleTypeDef
#include <stdint.h>   // typy całkowite

/* Struktura danych z jednego odczytu (z filtrami) */
typedef struct {
    uint16_t distance;       /* [cm]  surowy dystans z rejestrów 0x00/0x01                */
    uint16_t distance_filt;  /* [cm]  dystans po medianie (okno z config)                 */
    uint16_t strength;       /* [raw] surowa siła sygnału z 0x02/0x03                     */
    uint16_t strength_filt;  /* [raw] siła po średniej kroczącej (okno z config)          */
    float    temperature;    /* [°C]  temp. układu: (int16_t(0x05:0x04) / 100.0f) → 0.1°C */
    uint8_t  frameReady;     /* 1 = nowy poprawny odczyt; 0 = brak (zwracamy ostatnie filtry) */
} TF_LunaData_t;

/* Inicjalizacja komunikacji dla prawego i lewego czujnika (zapamiętujemy uchwyty I²C) */
void          TF_Luna_Right_Init(I2C_HandleTypeDef *hi2c1);
void          TF_Luna_Left_Init (I2C_HandleTypeDef *hi2c3);

/* Odczyt pojedynczej ramki (rejestry 0x00..0x05 + filtracja) */
TF_LunaData_t TF_Luna_Right_Read(void);
TF_LunaData_t TF_Luna_Left_Read (void);

/* Szacowanie temperatury otoczenia: module °C + offset z CFG_Luna()->temp_offset_c (0.1°C) */
float         TF_Luna_AmbientEstimateC(const TF_LunaData_t *d);

#ifdef __cplusplus
}
#endif
#endif /* TF_LUNA_I2C_H_ */
