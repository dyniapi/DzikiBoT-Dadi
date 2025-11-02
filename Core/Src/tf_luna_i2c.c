/**
 * @file    tf_luna_i2c.c
 * @brief   Obsługa TF-Luna (I²C): odczyt ramek, filtry i diagnostyka.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 *
 * Funkcje w pliku (skrót):
 *   - TF_Luna_Right_Init(I2C_HandleTypeDef *hi2c1)
 *   - TF_Luna_Left_Init(I2C_HandleTypeDef *hi2c3)
 *   - TF_Luna_Right_ResetFilters(void)
 *   - TF_Luna_Left_ResetFilters(void)
 *   - absf_fast(float x)
 *   - tfluna_checksum_ok(const uint8_t *buf, uint8_t len)
 *   - tfluna_decode_tempC(int16_t raw, float last_good)
 *   - median_u16(const uint16_t *arr, uint8_t n)
 *   - mean_u16(const uint16_t *arr, uint8_t n)
 *   - filt_update_cfg(tfluna_filt_t *f, uint16_t dist, uint16_t str,
                            uint16_t *out_med, uint16_t *out_ma)
 *   - tfluna_read_regs(I2C_HandleTypeDef *hi2c, TF_LunaData_t *out, tfluna_filt_t *fs)
 *   - tfluna_read_burst(I2C_HandleTypeDef *hi2c, TF_LunaData_t *out, tfluna_filt_t *fs)
 *   - TF_Luna_Read_Generic(I2C_HandleTypeDef *hi2c, tfluna_filt_t *fs)
 *   - TF_Luna_Right_Read(void)
 *   - TF_Luna_Left_Read(void)
 */

#include "tf_luna_i2c.h"
#include "config.h"
#include <string.h>
#include "stm32l4xx_hal.h"

/* ───────────────────── Ustawienia magistrali ───────────────────── */

#define TFLUNA_ADDR               (0x10u << 1)  /* 7-bit 0x10 → 8-bit HAL */
#define TFLUNA_READ_TRIES         3u
#define TFLUNA_I2C_TIMEOUT_TX     10u
#define TFLUNA_I2C_TIMEOUT_RX     10u

/* ───────────────────── Uchwyty I2C dla prawego/lewego ───────────────────── */

static I2C_HandleTypeDef *luna_right = NULL; // I2C1
static I2C_HandleTypeDef *luna_left  = NULL; // I2C3

void TF_Luna_Right_Init(I2C_HandleTypeDef *hi2c1) { luna_right = hi2c1; }
void TF_Luna_Left_Init (I2C_HandleTypeDef *hi2c3) { luna_left  = hi2c3; }

/* ───────────────────── Stan filtrów per-sensor ─────────────────────
 * Zostawiamy bufor o stałej wielkości 5 (tak jak w Twoim działającym kodzie).
 * Okna MED/MA bierzemy z config, ale CLAMP do 1..5 (MED dodatkowo → nieparzyste).
 */
#define WIN_MAX 5u

typedef struct {
    uint16_t dist_hist[WIN_MAX];
    uint16_t str_hist [WIN_MAX];
    uint8_t  count;      /* ile już mamy (<= WIN_MAX) */
    uint8_t  idx;        /* pozycja do nadpisania w pierścieniu */
    float    last_tempC; /* ostatnia dobra temp (°C) */
    uint16_t last_med;   /* ostatni MED distance */
    uint16_t last_ma;    /* ostatni MA strength  */
} tfluna_filt_t;

static tfluna_filt_t filt_right = {0};
static tfluna_filt_t filt_left  = {0};

void TF_Luna_Right_ResetFilters(void) { memset(&filt_right, 0, sizeof(filt_right)); }
void TF_Luna_Left_ResetFilters(void)  { memset(&filt_left,  0, sizeof(filt_left));  }

/* ───────────────────── Pomocnicze ───────────────────── */

static inline float absf_fast(float x) { return (x < 0.0f) ? -x : x; }

/* Suma kontrolna: checksum = (sum buf[0..len-2]) & 0xFF == buf[len-1] */
static uint8_t tfluna_checksum_ok(const uint8_t *buf, uint8_t len)
{
    if (!buf || len < 2u) {
        return 0u;
    }
    uint16_t s = 0;
    for (uint8_t i = 0; i < (uint8_t)(len - 1u); ++i) {
        s += buf[i];
    }
    return ((s & 0xFFu) == buf[len - 1u]) ? 1u : 0u;
}

/* Heurystyka temperatury (jak w Twoim pliku):
   próbujemy (raw/8 - 256) oraz (raw/10.0), wybieramy sensowniejszą. */
static float tfluna_decode_tempC(int16_t raw, float last_good)
{
    float t1 = (raw / 8.0f) - 256.0f; /* klasyczne */
    float t2 = (float)raw / 10.0f;    /* „dziesiąte °C” – spotykane w rewizjach I2C */

    uint8_t ok1 = (t1 > -50.0f && t1 < 100.0f);
    uint8_t ok2 = (t2 > -50.0f && t2 < 100.0f);

    float chosen;
    if (ok1 && ok2) {
        if (last_good == 0.0f) {
            chosen = (t1 > -30.0f && t1 < 80.0f) ? t1 : t2;
        } else {
            chosen = (absf_fast(t1 - last_good) < absf_fast(t2 - last_good)) ? t1 : t2;
        }
    } else if (ok1) {
        chosen = t1;
    } else if (ok2) {
        chosen = t2;
    } else {
        chosen = (last_good == 0.0f) ? 25.0f : last_good;
    }

    /* skala z config (u Ciebie powinna być 1.0f) */
    chosen *= CFG_Luna()->temp_scale;

    /* clamp do granic pracy modułu */
    if (chosen < -30.0f) {
        chosen = -30.0f;
    }
    if (chosen > 85.0f) {
        chosen = 85.0f;
    }
    return chosen;
}

/* median z N (N<=WIN_MAX). Kopiuje lokalnie i sortuje prostym wstawianiem. */
static uint16_t median_u16(const uint16_t *arr, uint8_t n)
{
    uint16_t tmp[WIN_MAX];
    for (uint8_t i = 0; i < n; ++i) {
        tmp[i] = arr[i];
    }
    for (uint8_t i = 1; i < n; ++i) {
        uint16_t key = tmp[i];
        int j = (int)i - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];
}

static uint16_t mean_u16(const uint16_t *arr, uint8_t n)
{
    uint32_t s = 0;
    for (uint8_t i = 0; i < n; ++i) {
        s += arr[i];
    }
    return (uint16_t)(s / (uint32_t)n);
}

/* aktualizacja filtrów; MED/MA – okna z config, ale CLAMP do 1..WIN_MAX */
static void filt_update_cfg(tfluna_filt_t *f, uint16_t dist, uint16_t str,
                            uint16_t *out_med, uint16_t *out_ma)
{
    const ConfigLuna_t *L = CFG_Luna();

    uint8_t wmed = L->median_win;
    if (wmed < 1u) { wmed = 1u; }
    if (wmed > WIN_MAX) { wmed = WIN_MAX; }
    if ((wmed & 1u) == 0u) { wmed--; } /* MED = okno nieparzyste */

    uint8_t wma = L->ma_win;
    if (wma < 1u) { wma = 1u; }
    if (wma > WIN_MAX) { wma = WIN_MAX; }

    /* wpis do buforów pierścieniowych */
    f->dist_hist[f->idx] = dist;
    f->str_hist [f->idx] = str;

    if (f->count < WIN_MAX) {
        f->count++;
    }
    f->idx = (uint8_t)((f->idx + 1u) % WIN_MAX);

    /* realna liczba elementów (gdy na starcie bufor niepełny) */
    uint8_t nmed = (f->count < wmed) ? f->count : wmed;
    uint8_t nma  = (f->count < wma ) ? f->count : wma;

    uint16_t med = median_u16(f->dist_hist, nmed);
    uint16_t ma  = mean_u16  (f->str_hist,  nma );

    f->last_med = med;
    f->last_ma  = ma;

    if (out_med) { *out_med = med; }
    if (out_ma)  { *out_ma  = ma;  }
}

/* Odczyt rejestrami: Dist L/H od 0x00, Str L/H 0x02/0x03, Temp L/H 0x04/0x05 */
static uint8_t tfluna_read_regs(I2C_HandleTypeDef *hi2c, TF_LunaData_t *out, tfluna_filt_t *fs)
{
    if (!hi2c || !out || !fs) {
        return 0u;
    }

    uint8_t reg = 0x00u;
    uint8_t data[6] = {0};

    if (HAL_I2C_Master_Transmit(hi2c, TFLUNA_ADDR, &reg, 1, TFLUNA_I2C_TIMEOUT_TX) != HAL_OK) {
        return 0u;
    }
    if (HAL_I2C_Master_Receive(hi2c, TFLUNA_ADDR, data, sizeof(data), TFLUNA_I2C_TIMEOUT_RX) != HAL_OK) {
        return 0u;
    }

    uint16_t dist     = (uint16_t)(data[0] | (data[1] << 8));
    uint16_t strength = (uint16_t)(data[2] | (data[3] << 8));
    int16_t  traw     = (int16_t)(data[4] | (data[5] << 8));

    out->distance    = dist;
    out->strength    = strength;
    out->temperature = tfluna_decode_tempC(traw, fs->last_tempC);
    fs->last_tempC   = out->temperature;

    /* filtry z oknami z config */
    filt_update_cfg(fs, dist, strength, &out->distance_filt, &out->strength_filt);

    out->frameReady  = 1u;
    return 1u;
}

/* Odczyt „burst” 9 bajtów – [0]=0x59 [1]=0x59 [2..7]=payload [8]=sum */
static uint8_t tfluna_read_burst(I2C_HandleTypeDef *hi2c, TF_LunaData_t *out, tfluna_filt_t *fs)
{
    if (!hi2c || !out || !fs) {
        return 0u;
    }

    uint8_t reg0 = 0x00u;
    uint8_t buf[9] = {0};

    if (HAL_I2C_Master_Transmit(hi2c, TFLUNA_ADDR, &reg0, 1, TFLUNA_I2C_TIMEOUT_TX) != HAL_OK) {
        return 0u;
    }
    if (HAL_I2C_Master_Receive(hi2c, TFLUNA_ADDR, buf, sizeof(buf), TFLUNA_I2C_TIMEOUT_RX) != HAL_OK) {
        return 0u;
    }

    /* nagłówek + checksum */
    if (!(buf[0] == 0x59u && buf[1] == 0x59u)) {
        return 0u;
    }
    if (!tfluna_checksum_ok(buf, 9)) {
        return 0u;
    }

    uint16_t dist     = (uint16_t)(buf[2] | (buf[3] << 8));
    uint16_t strength = (uint16_t)(buf[4] | (buf[5] << 8));
    int16_t  traw     = (int16_t)(buf[6] | (buf[7] << 8));

    out->distance    = dist;
    out->strength    = strength;
    out->temperature = tfluna_decode_tempC(traw, fs->last_tempC);
    fs->last_tempC   = out->temperature;

    /* filtry z oknami z config */
    filt_update_cfg(fs, dist, strength, &out->distance_filt, &out->strength_filt);

    out->frameReady  = 1u;
    return 1u;
}

/* Główny odczyt: burst → rejestry; kilka prób. Przy braku nowej ramki
   zwracamy „ostatnie stabilne” (frameReady=0), żeby UI nie pokazywało „0”. */
static TF_LunaData_t TF_Luna_Read_Generic(I2C_HandleTypeDef *hi2c, tfluna_filt_t *fs)
{
    TF_LunaData_t out;
    memset(&out, 0, sizeof(out));

    if (!hi2c || !fs) {
        return out;
    }

    for (uint8_t attempt = 0; attempt < TFLUNA_READ_TRIES; ++attempt) {
        if (tfluna_read_burst(hi2c, &out, fs)) {
            return out;
        }
        if (tfluna_read_regs (hi2c, &out, fs)) {
            return out;
        }
        HAL_Delay(2);
    }

    /* NO FRAME – użyj ostatnich filtrów + ostatniej temp */
    out.distance_filt = fs->last_med;
    out.strength_filt = fs->last_ma;
    out.temperature   = (fs->last_tempC == 0.0f) ? 25.0f : fs->last_tempC;
    out.frameReady    = 0u;
    return out;
}

/* Publiczne API – aliasy na „prawy/lewy” czujnik, + offsety z config */
TF_LunaData_t TF_Luna_Right_Read(void)
{
    TF_LunaData_t d = TF_Luna_Read_Generic(luna_right, &filt_right);
    /* offset po filtrze, żeby nie zniekształcać mediany/średniej */
    int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_right_mm;
    if (v < 0) { v = 0; }
    if (v > 65535) { v = 65535; }
    d.distance = (uint16_t)v;
    return d;
}

TF_LunaData_t TF_Luna_Left_Read(void)
{
    TF_LunaData_t d = TF_Luna_Read_Generic(luna_left, &filt_left);
    int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_left_mm;
    if (v < 0) { v = 0; }
    if (v > 65535) { v = 65535; }
    d.distance = (uint16_t)v;
    return d;
}
