/**
 * ============================================================================
 *  MODULE: tcs3472.c — RAW C/R/G/B + auto-gain + EMA (bez zmian API)
 * -----------------------------------------------------------------------------
 *  API:
 *    void           TCS3472_Right_Init(I2C_HandleTypeDef *hi2c1);
 *    void           TCS3472_Left_Init (I2C_HandleTypeDef *hi2c3);
 *    TCS3472_Data_t TCS3472_Right_Read(void);
 *    TCS3472_Data_t TCS3472_Left_Read (void);
 *    void           TCS3472_Config    (I2C_HandleTypeDef *hi2c);
 *
 *  MECHANIKA:
 *    - Histereza auto-gain na Clear (progi z getterów CFG_TCS_AG_*()).
 *    - EMA na C/R/G/B (alfa z CFG_TCS_EMA_Alpha()), z kompensacją przy zmianie gainu.
 *    - I²C transakcje krótkie; bez opóźnień blokujących.
 * ============================================================================
 */

#include "tcs3472.h"
#include "config.h"
#include "stm32l4xx_hal.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* --- Adresy/rejestry --- */
#define TCS3472_ADDR   (0x29u << 1)
#define CMD(x)         (0x80u | (x))
#define REG_ENABLE     0x00u
#define REG_ATIME      0x01u
#define REG_CONTROL    0x0Fu
#define REG_ID         0x12u
#define REG_CDATAL     0x14u   /* 8 B: C,R,G,B (LSB,MSB) */

#define TCS_FS_16      (65535u)  // pełna skala 16-bit

/* --- (weak) gettery tuningu — można nadpisać w config.c --- */
__attribute__((weak)) float CFG_TCS_EMA_Alpha(void) { return 0.30f; }
__attribute__((weak)) float CFG_TCS_AG_LoPct(void)  { return 0.60f; }
__attribute__((weak)) float CFG_TCS_AG_HiPct(void)  { return 0.70f; }

/* --- Uchwyty I²C przypisane do stron --- */
static I2C_HandleTypeDef *tcs_right = NULL;  // Right  (I2C1)
static I2C_HandleTypeDef *tcs_left  = NULL;  // Left   (I2C3)

/* --- Stan per strona --- */
typedef struct {
    I2C_HandleTypeDef *bus;      // magistrala I²C
    TCS_Gain_t         gain;     // aktualny gain
    float ema_c, ema_r, ema_g, ema_b; // stan EMA
    uint8_t ema_init;            // 0=niezainicjalizowany, 1=zainicjalizowany
} TCS_State_t;

static TCS_State_t s_right = {0};
static TCS_State_t s_left  = {0};

/* --- (weak) hook: log zmiany gainu --- */
__attribute__((weak)) void TCS3472_OnGainChange(const char* side, TCS_Gain_t oldg, TCS_Gain_t newg)
{
    (void)side; (void)oldg; (void)newg; // domyślnie nic
}

/* --- Helpery: atime/gain/reg --- */
static uint8_t tcs_atime_from_ms(float ms)
{
    if (ms < 2.4f)   ms = 2.4f;          // dół zakresu
    if (ms > 614.0f) ms = 614.0f;        // góra zakresu
    int reg = (int)(256.0f - (ms / 2.4f) + 0.5f);
    if (reg < 0)   reg = 0;
    if (reg > 255) reg = 255;
    return (uint8_t)reg;
}
static inline uint8_t tcs_gain_to_reg(TCS_Gain_t g)
{
    switch (g) {
        case TCS_GAIN_1X:  return 0x00;
        case TCS_GAIN_4X:  return 0x01;
        case TCS_GAIN_16X: return 0x02;
        case TCS_GAIN_60X: return 0x03;
        default:           return 0x01;
    }
}
static inline float tcs_gain_multiplier(TCS_Gain_t g)
{
    switch (g) {
        case TCS_GAIN_1X:  return 1.0f;
        case TCS_GAIN_4X:  return 4.0f;
        case TCS_GAIN_16X: return 16.0f;
        case TCS_GAIN_60X: return 60.0f;
        default:           return 4.0f;
    }
}

/* --- I²C IO --- */
static void tcs_write_u8(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val)
{
    uint8_t cmd[2] = { CMD(reg), val };                 // [CMD|reg, val]
    (void)HAL_I2C_Master_Transmit(hi2c, TCS3472_ADDR, cmd, 2, 20u);
}
static TCS3472_Data_t tcs_read_raw(I2C_HandleTypeDef *hi2c)
{
    TCS3472_Data_t d = (TCS3472_Data_t){0};
    if (!hi2c) return d;

    uint8_t reg = CMD(REG_CDATAL);                      // bazowy rejestr danych
    uint8_t buf[8];                                     // 4×(LSB,MSB)

    if (HAL_I2C_Master_Transmit(hi2c, TCS3472_ADDR, &reg, 1, 20u) != HAL_OK) return d;
    if (HAL_I2C_Master_Receive (hi2c, TCS3472_ADDR, buf, sizeof(buf), 20u) != HAL_OK) return d;

    d.clear = (uint16_t)(buf[0] | (buf[1] << 8));
    d.red   = (uint16_t)(buf[2] | (buf[3] << 8));
    d.green = (uint16_t)(buf[4] | (buf[5] << 8));
    d.blue  = (uint16_t)(buf[6] | (buf[7] << 8));
    return d;
}

/* --- Zmiana gainu z kompensacją EMA + hook --- */
static void tcs_set_gain(TCS_State_t *S, TCS_Gain_t new_gain)
{
    if (!S || (S->gain == new_gain)) return;

    const TCS_Gain_t oldg = S->gain;                     // zapamiętaj stary gain
    const float old_m = tcs_gain_multiplier(S->gain);    // krotność starego gainu
    const float new_m = tcs_gain_multiplier(new_gain);   // krotność nowego gainu
    const float k     = (new_m > 0.0f) ? (old_m / new_m) : 1.0f;

    S->ema_c *= k; S->ema_r *= k; S->ema_g *= k; S->ema_b *= k; // anty-skoki
    tcs_write_u8(S->bus, REG_CONTROL, tcs_gain_to_reg(new_gain));
    S->gain = new_gain;

    const char* side = (S == &s_right) ? "Right" : (S == &s_left) ? "Left" : "?";
    TCS3472_OnGainChange(side, oldg, new_gain);          // opcjonalny log
}

/* --- Konfiguracja rejestrów (publiczna) --- */
void TCS3472_Config(I2C_HandleTypeDef *hi2c)
{
    if (!hi2c) return;

    const ConfigTCS_t *T = CFG_TCS();                    // atime/gain startowe
    tcs_write_u8(hi2c, REG_ENABLE,  0x03u);              // PON | AEN
    tcs_write_u8(hi2c, REG_ATIME,   tcs_atime_from_ms(T->atime_ms));
    tcs_write_u8(hi2c, REG_CONTROL, tcs_gain_to_reg(T->gain));

    if (hi2c == tcs_right) {                             // reset stanu (Right)
        s_right.gain = T->gain;
        s_right.ema_c = s_right.ema_r = s_right.ema_g = s_right.ema_b = 0.0f;
        s_right.ema_init = 0u;
    } else if (hi2c == tcs_left) {                       // reset stanu (Left)
        s_left.gain  = T->gain;
        s_left.ema_c = s_left.ema_r = s_left.ema_g = s_left.ema_b = 0.0f;
        s_left.ema_init = 0u;
    }
}

/* --- Init Right/Left (API bez zmian) --- */
void TCS3472_Right_Init(I2C_HandleTypeDef *hi2c1)
{
    tcs_right = hi2c1; s_right.bus = hi2c1; s_right.gain = CFG_TCS()->gain;
    s_right.ema_c = s_right.ema_r = s_right.ema_g = s_right.ema_b = 0.0f; s_right.ema_init = 0u;
    TCS3472_Config(hi2c1);
}
void TCS3472_Left_Init(I2C_HandleTypeDef *hi2c3)
{
    tcs_left  = hi2c3; s_left.bus  = hi2c3; s_left.gain  = CFG_TCS()->gain;
    s_left.ema_c  = s_left.ema_r  = s_left.ema_g  = s_left.ema_b  = 0.0f;  s_left.ema_init  = 0u;
    TCS3472_Config(hi2c3);
}

/* --- EMA helper --- */
static inline float ema_update(float y, float x, float a) { return y + a * (x - y); }

/* --- Rdzeń: odczyt + auto-gain + EMA --- */
static TCS3472_Data_t TCS3472_Process(TCS_State_t *S)
{
    TCS3472_Data_t out = (TCS3472_Data_t){0};
    if (!S || !S->bus) return out;

    /* parametry tuningu z configu (mogą być nadpisane) */
    const float a = CFG_TCS_EMA_Alpha();
    float lo = CFG_TCS_AG_LoPct(), hi = CFG_TCS_AG_HiPct();
    if (lo < 0.05f) lo = 0.05f;             // sanity
    if (hi > 0.95f) hi = 0.95f;
    if (hi < lo + 0.02f) hi = lo + 0.02f;   // min. 2% histerezy
    const uint32_t thr_lo = (uint32_t)(lo * (float)TCS_FS_16 + 0.5f);
    const uint32_t thr_hi = (uint32_t)(hi * (float)TCS_FS_16 + 0.5f);

    /* surowy odczyt */
    const TCS3472_Data_t raw = tcs_read_raw(S->bus);

    /* auto-gain (Clear) */
    if (raw.clear > thr_hi) {
        if      (S->gain == TCS_GAIN_16X) tcs_set_gain(S, TCS_GAIN_4X);
        else if (S->gain == TCS_GAIN_4X)  tcs_set_gain(S, TCS_GAIN_1X);
        else if (S->gain == TCS_GAIN_1X)  { /* min */ }
        else /*60X*/                      tcs_set_gain(S, TCS_GAIN_16X); // zejście z 60×
    } else if (raw.clear < thr_lo) {
        if      (S->gain == TCS_GAIN_1X)  tcs_set_gain(S, TCS_GAIN_4X);
        else if (S->gain == TCS_GAIN_4X)  tcs_set_gain(S, TCS_GAIN_16X);
        else if (S->gain == TCS_GAIN_16X) tcs_set_gain(S, TCS_GAIN_60X);
        else /*60X*/                      { /* max */ }
    }

    /* EMA (pierwsza próbka = init bez opóźnienia) */
    if (!S->ema_init) {
        S->ema_c = (float)raw.clear; S->ema_r = (float)raw.red; S->ema_g = (float)raw.green; S->ema_b = (float)raw.blue;
        S->ema_init = 1u;
    } else {
        S->ema_c = ema_update(S->ema_c, (float)raw.clear, a);
        S->ema_r = ema_update(S->ema_r, (float)raw.red,   a);
        S->ema_g = ema_update(S->ema_g, (float)raw.green, a);
        S->ema_b = ema_update(S->ema_b, (float)raw.blue,  a);
    }

    /* saturacja i zwrot */
    out.clear = (uint16_t)(S->ema_c < 0.0f ? 0.0f : (S->ema_c > (float)TCS_FS_16 ? (float)TCS_FS_16 : S->ema_c));
    out.red   = (uint16_t)(S->ema_r < 0.0f ? 0.0f : (S->ema_r > (float)TCS_FS_16 ? (float)TCS_FS_16 : S->ema_r));
    out.green = (uint16_t)(S->ema_g < 0.0f ? 0.0f : (S->ema_g > (float)TCS_FS_16 ? (float)TCS_FS_16 : S->ema_g));
    out.blue  = (uint16_t)(S->ema_b < 0.0f ? 0.0f : (S->ema_b > (float)TCS_FS_16 ? (float)TCS_FS_16 : S->ema_b));
    return out;
}

/* publiczne odczyty */
TCS3472_Data_t TCS3472_Right_Read(void) { return TCS3472_Process(&s_right); }
TCS3472_Data_t TCS3472_Left_Read (void) { return TCS3472_Process(&s_left);  }
