/**
 * @file    ssd1306.h
 * @brief   Wyświetlacz SSD1306 i panel diagnostyczny — prezentacja danych.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 */

#ifndef SSD1306_H_
#define SSD1306_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ====== Konfiguracja sprzętowa ====== */
#include "main.h"
#include "i2c.h"     /* split-CubeMX: zapewnia extern I2C_HandleTypeDef hi2c1 */

#ifndef SSD1306_I2C_ADDR
#define SSD1306_I2C_ADDR     (0x3C << 1)  /* typowe 0x3C; czasem 0x3D */
#endif

#ifndef SSD1306_I2C_HANDLE
#define SSD1306_I2C_HANDLE   hi2c1        /* domyślnie korzystamy z I2C1 */
#endif

/* ====== Parametry wyświetlacza ====== */
#define SSD1306_WIDTH        128
#define SSD1306_HEIGHT       64
#define SSD1306_PAGES        (SSD1306_HEIGHT/8) /* = 8 */
#define SSD1306_BUF_SIZE     (SSD1306_WIDTH * SSD1306_PAGES)

/* ====== API wysokiego i niskiego poziomu ====== */

/* Inicjalizacja / zasilenie / konfiguracja kontrolera */
void SSD1306_Init(void);

/* Czyszczenie bufora i odświeżanie ekranu */
void SSD1306_Clear(void);
void SSD1306_UpdateScreen(void);

/* Rysowanie tekstu 6x8 (ASCII 0x20..0x7F) */
void SSD1306_DrawChar(uint8_t x, uint8_t page, char c);
void SSD1306_DrawText(uint8_t page, const char *text);                 /* x=0 */
void SSD1306_DrawTextAt(uint8_t page, uint8_t x, const char *text);    /* x=0..127 */

/* Drobne narzędzia rysujące */
void SSD1306_SetContrast(uint8_t value);        /* 0..255 */
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t on);  /* on: 0/1 */
void SSD1306_DrawHLine(uint8_t y, uint8_t x0, uint8_t x1);

/* Panel diagnostyczny dla DzikiBoT (4 linie) */
#include "tf_luna_i2c.h"
#include "tcs3472.h"
void OLED_ShowSensors(const TF_LunaData_t *RightLuna,
                      const TF_LunaData_t *LeftLuna,
                      const TCS3472_Data_t *RightColor,
                      const TCS3472_Data_t *LeftColor);

#ifdef __cplusplus
}
#endif
#endif /* SSD1306_H_ */
