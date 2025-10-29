/**
 ******************************************************************************
 * @file    ssd1306.h
 * @brief   Sterownik OLED SSD1306 (128x64, I2C) + czcionka ASCII 6x8
 * @version 1.0
 * @date    2025-10-24
 *
 * Funkcje wysokiego poziomu:
 *  - SSD1306_Init()                 : inicjalizacja wyświetlacza
 *  - SSD1306_Clear()                : czyszczenie bufora ekranu (na czarno)
 *  - SSD1306_UpdateScreen()         : wysłanie bufora do OLED
 *  - SSD1306_DrawText(page, str)    : napis od x=0 w podanej stronie (co 8 px pionowo)
 *  - SSD1306_DrawTextAt(page, x, s) : napis od (x, page)
 *  - SSD1306_SetContrast(val)       : kontrast (0..255)
 *  - SSD1306_DrawHLine(y, x0, x1)   : pozioma linia na wysokości y
 *  - OLED_ShowSensors(...)          : panel diagnostyczny 4-liniowy
 *
 * Uwaga o „page”:
 *  - Wysokość 64 px = 8 „stron” po 8 px: page = 0..7 (typowo używamy 0,2,4,6 dla 4 linii)
 ******************************************************************************
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
