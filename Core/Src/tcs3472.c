/**
 * @file    tcs3472.c
 * @brief   TCS3472 – Right=I2C1, Left=I2C3; konfiguracja z config.[ch]
 * @note    Sterownik zwraca SUROWE C/R/G/B. Skalowanie do prezentacji (/64)
 *          zostaje w UI (DebugUART/OLED), żeby uniknąć podwójnego dzielenia.
 */

#include "tcs3472.h"
#include "config.h"
#include <string.h>
#include "stm32l4xx_hal.h"

/* Adres i makra */
#define TCS3472_ADDR   (0x29u << 1)
#define CMD(x)         (0x80u | (x))

/* Rejestry */
#define REG_ENABLE     0x00u
#define REG_ATIME      0x01u
#define REG_CONTROL    0x0Fu
#define REG_ID         0x12u
#define REG_CDATAL     0x14u   /* 8 bajtów: C,R,G,B (LSB,MSB) */

static I2C_HandleTypeDef *tcs_right = NULL; /* I2C1 */
static I2C_HandleTypeDef *tcs_left  = NULL; /* I2C3 */

/* ms → ATIME (time ≈ (256-ATIME)*2.4 ms) */
static uint8_t tcs_atime_from_ms(float ms)
{
    if (ms < 2.4f) { ms = 2.4f; }
    if (ms > 614.0f) { ms = 614.0f; }
    int reg = (int)(256.0f - (ms / 2.4f) + 0.5f);
    if (reg < 0) { reg = 0; }
    if (reg > 255) { reg = 255; }
    return (uint8_t)reg;
}

static uint8_t tcs_gain_to_reg(TCS_Gain_t g)
{
    switch (g) {
        case TCS_GAIN_1X:  return 0x00;
        case TCS_GAIN_4X:  return 0x01;
        case TCS_GAIN_16X: return 0x02;
        case TCS_GAIN_60X: return 0x03;
        default:           return 0x01;
    }
}

/* Konfiguracja wg config */
static void TCS3472_Config(I2C_HandleTypeDef *hi2c)
{
    if (!hi2c) { return; }

    const ConfigTCS_t *T = CFG_TCS();
    uint8_t atime = tcs_atime_from_ms(T->atime_ms);
    uint8_t gain  = tcs_gain_to_reg(T->gain);

    uint8_t cmd[2];

    cmd[0] = CMD(REG_ENABLE);  cmd[1] = 0x03u;  /* PON | AEN */
    HAL_I2C_Master_Transmit(hi2c, TCS3472_ADDR, cmd, 2, 20);

    cmd[0] = CMD(REG_ATIME);   cmd[1] = atime;
    HAL_I2C_Master_Transmit(hi2c, TCS3472_ADDR, cmd, 2, 20);

    cmd[0] = CMD(REG_CONTROL); cmd[1] = gain;
    HAL_I2C_Master_Transmit(hi2c, TCS3472_ADDR, cmd, 2, 20);
}

/* Inicjalizacja P/L */
void TCS3472_Right_Init(I2C_HandleTypeDef *hi2c1) { tcs_right = hi2c1; TCS3472_Config(tcs_right); }
void TCS3472_Left_Init (I2C_HandleTypeDef *hi2c3) { tcs_left  = hi2c3; TCS3472_Config(tcs_left ); }

/* Odczyt bloku C/R/G/B */
static TCS3472_Data_t TCS3472_ReadGeneric(I2C_HandleTypeDef *hi2c)
{
    TCS3472_Data_t d = {0};
    if (!hi2c) { return d; }

    uint8_t reg = CMD(REG_CDATAL);
    uint8_t buf[8] = {0};

    if (HAL_I2C_Master_Transmit(hi2c, TCS3472_ADDR, &reg, 1, 20) != HAL_OK) { return d; }
    if (HAL_I2C_Master_Receive(hi2c, TCS3472_ADDR, buf, sizeof(buf), 20) != HAL_OK) { return d; }

    d.clear = (uint16_t)(buf[0] | (buf[1] << 8));
    d.red   = (uint16_t)(buf[2] | (buf[3] << 8));
    d.green = (uint16_t)(buf[4] | (buf[5] << 8));
    d.blue  = (uint16_t)(buf[6] | (buf[7] << 8));
    return d;
}

TCS3472_Data_t TCS3472_Right_Read(void) { return TCS3472_ReadGeneric(tcs_right); }
TCS3472_Data_t TCS3472_Left_Read (void) { return TCS3472_ReadGeneric(tcs_left ); }
