/**
 * @file    oled_panel.h
 * @brief   Wyświetlacz SSD1306 i panel diagnostyczny — prezentacja danych.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 */

#ifndef OLED_PANEL_H
#define OLED_PANEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "tf_luna_i2c.h"   // TF_LunaData_t
#include "tcs3472.h"       // TCS3472_Data_t

/* Rysuje panel 7 linii na SSD1306.
   Zakłada, że SSD1306_Init() było już wywołane i magistrala I2C działa. */
void OLED_Panel_ShowSensors(const TF_LunaData_t *R,
                            const TF_LunaData_t *L,
                            const TCS3472_Data_t *CR,
                            const TCS3472_Data_t *CL);

#ifdef __cplusplus
}
#endif
#endif /* OLED_PANEL_H */
