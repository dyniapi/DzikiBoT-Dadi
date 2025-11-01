/**
 * @file    tf_luna_i2c.c
 * @brief   TF-Luna (I2C) – dwa czujniki: Right=I2C1, Left=I2C3
 *
 * Ramka 9 bajtów:
 *   [0]=0x59 [1]=0x59 [2]=DistL [3]=DistH [4]=StrL [5]=StrH [6]=TempL [7]=TempH [8]=Sum
 * Suma: (b0..b7) mod 256 == b8
 *
 * Wersja po przeniesieniu parametrów do config.[ch]:
 *  - okna filtrów (median/MA), skala temperatury, offsety dystansu → CFG_Luna()
 *  - większe time-outy oraz kilka prób odczytu
 *  - potrójna próba odczytu: Mem_Read(0x00) → Master_Receive(9B) → skan w 18B
 *  - domyślnie NIE blokujemy wyświetlania progami, by „ożywić” UI
 *    (jedna linia do odkomentowania, jeśli chcesz filtrować po strength/range)
 *
 * Zwraca:
 *  - .distance/.strength – JUŻ po filtrach (stabilne do OLED/UART)
 *  - .temperature        – °C (formuła producenta * temp_scale)
 *  - .frameReady         – 1 tylko dla bieżącej, poprawnej ramki
 */

#include "tf_luna_i2c.h"
#include "config.h"
#include "stm32l4xx_hal.h"
#include <string.h>

/* ---------- Adres I2C ---------- */
#define TFLUNA_ADDR        (0x10u << 1)   /* 7-bit 0x10 → 8-bit HAL */

/* ---------- Próby i timeouty ---------- */
#define TFLUNA_TRIES       5u
#define TFLUNA_TOUT_RX     80u            /* większy time-out pomaga przy wolniejszych ramkach */

/* ---------- Lokalne uchwyty ---------- */
static I2C_HandleTypeDef *luna_right = NULL;  /* I2C1 (PB6/PB7) */
static I2C_HandleTypeDef *luna_left  = NULL;  /* I2C3 (PC0/PC1) */

/* ---------- Bufory filtrów (max okno 9) ---------- */
#define WIN_MAX  9

typedef struct {
    uint16_t dist_hist[WIN_MAX];
    uint16_t str_hist [WIN_MAX];
    uint8_t  idx;
    uint8_t  count;
    /* „ostatnie dobre” po filtrze – do NO FRAME */
    uint16_t last_dist;
    uint16_t last_str;
} luna_filt_t;

static luna_filt_t filt_R = {0};
static luna_filt_t filt_L = {0};

/* ---------- API: inicjalizacja i (opcjonalnie) reset filtrów ---------- */
void TF_Luna_Right_Init(I2C_HandleTypeDef *hi2c1)
{
    luna_right = hi2c1;
    memset(&filt_R, 0, sizeof(filt_R));
}
void TF_Luna_Left_Init(I2C_HandleTypeDef *hi2c3)
{
    luna_left = hi2c3;
    memset(&filt_L, 0, sizeof(filt_L));
}
void TF_Luna_Right_ResetFilters(void) { memset(&filt_R, 0, sizeof(filt_R)); }
void TF_Luna_Left_ResetFilters(void)  { memset(&filt_L, 0, sizeof(filt_L)); }

/* ---------- proste stateless MED/MA (n ≤ 9) ---------- */
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

/* ---------- aktualizacja filtrów wg config ---------- */
static void filters_update(luna_filt_t *f, uint16_t dist, uint16_t str,
                           uint16_t *dist_out, uint16_t *str_out)
{
    const ConfigLuna_t *L = CFG_Luna();

    /* okna: clamp i MED wymuszone na nieparzyste */
    uint8_t wmed = L->median_win;
    if (wmed < 1) {
        wmed = 1;
    }
    if (wmed > WIN_MAX) {
        wmed = WIN_MAX;
    }
    if ((wmed & 1u) == 0u) {
        wmed--;
    }

    uint8_t wma = L->ma_win;
    if (wma < 1) {
        wma = 1;
    }
    if (wma > WIN_MAX) {
        wma = WIN_MAX;
    }

    /* wpis do bufora kołowego */
    f->dist_hist[f->idx] = dist;
    f->str_hist [f->idx] = str;
    f->idx = (uint8_t)((f->idx + 1u) % WIN_MAX);
    if (f->count < WIN_MAX) {
        f->count++;
    }

    uint8_t nmed = (f->count < wmed) ? f->count : wmed;
    uint8_t nma  = (f->count < wma ) ? f->count : wma;

    uint16_t d = median_u16(f->dist_hist, nmed);
    uint16_t s = mean_u16  (f->str_hist,  nma );

    f->last_dist = d;
    f->last_str  = s;

    if (dist_out) {
        *dist_out = d;
    }
    if (str_out) {
        *str_out  = s;
    }
}

/* ---------- walidacja ramki 9B ---------- */
static uint8_t frame_valid(const uint8_t *b)
{
    if (b[0] != 0x59u || b[1] != 0x59u) {
        return 0u;
    }
    uint16_t sum = 0;
    for (int i = 0; i < 8; ++i) {
        sum += b[i];
    }
    return ((uint8_t)(sum & 0xFFu)) == b[8];
}

/* ---------- próba odczytu 9B: 3 metody ---------- */
static uint8_t tfluna_try_read(I2C_HandleTypeDef *hi2c, uint8_t *out9)
{
    if (!hi2c || !out9) {
        return 0u;
    }

    /* 1) Mem_Read od 0x00 (wiele modułów tego oczekuje) */
    uint8_t b9[9] = {0};
    if (HAL_I2C_Mem_Read(hi2c, TFLUNA_ADDR, 0x00u, I2C_MEMADD_SIZE_8BIT,
                         b9, 9, TFLUNA_TOUT_RX) == HAL_OK) {
        if (frame_valid(b9)) {
            memcpy(out9, b9, 9);
            return 1u;
        }
    }

    /* 2) Zwykły Master_Receive(9) – część egzemplarzy tak działa */
    if (HAL_I2C_Master_Receive(hi2c, TFLUNA_ADDR, b9, 9, TFLUNA_TOUT_RX) == HAL_OK) {
        if (frame_valid(b9)) {
            memcpy(out9, b9, 9);
            return 1u;
        }
    }

    /* 3) „Dosynchronizacja”: weź 18 B i wyszukaj nagłówka 0x59 0x59 */
    uint8_t b18[18] = {0};
    if (HAL_I2C_Master_Receive(hi2c, TFLUNA_ADDR, b18, sizeof(b18), TFLUNA_TOUT_RX) == HAL_OK) {
        for (int i = 0; i <= (int)sizeof(b18) - 9; ++i) {
            if (b18[i] == 0x59u && b18[i + 1] == 0x59u) {
                if (frame_valid(&b18[i])) {
                    memcpy(out9, &b18[i], 9);
                    return 1u;
                }
            }
        }
    }

    return 0u;
}

/* ---------- wspólna funkcja odczytu + filtry + temp/offset ---------- */
static TF_LunaData_t TF_Luna_Read_Generic(I2C_HandleTypeDef *hi2c, luna_filt_t *f)
{
    TF_LunaData_t out = {0};
    if (!hi2c) {
        return out;
    }

    const ConfigLuna_t *L = CFG_Luna();

    uint8_t buf[9] = {0};
    uint8_t ok = 0u;

    for (uint8_t t = 0; t < TFLUNA_TRIES; ++t) {
        if (tfluna_try_read(hi2c, buf)) {
            ok = 1u;
            break;
        }
    }

    if (!ok) {
        /* NO FRAME – zostaw ostatnie stabilne (frameReady=0) */
        out.distance    = f->last_dist;
        out.strength    = f->last_str;
        out.temperature = 0.0f;
        out.frameReady  = 0u;
        return out;
    }

    /* Dekoduj surowe */
    uint16_t dist_raw     = (uint16_t)(buf[2] | (buf[3] << 8));
    uint16_t strength_raw = (uint16_t)(buf[4] | (buf[5] << 8));
    int16_t  temp_raw     = (int16_t)(buf[6] | (buf[7] << 8));

    /* Temperatura (°C) + skala z config (typ. 1.0f) */
    float temp_c = ((float)temp_raw / 8.0f) - 256.0f;
    temp_c *= L->temp_scale;

    /* Kwalifikacja progiem – DOMYŚLNIE WYŁĄCZONA, by UI nie widziało zer.
       Jeśli chcesz odrzucać „słabe”, odkomentuj: */
    /*
    if (strength_raw < L->strength_min || dist_raw < L->dist_min_mm || dist_raw > L->dist_max_mm) {
        out.distance    = f->last_dist;
        out.strength    = f->last_str;
        out.temperature = temp_c;
        out.frameReady  = 0u;
        return out;
    }
    */

    /* Filtry */
    uint16_t d_f = dist_raw, s_f = strength_raw;
    filters_update(f, dist_raw, strength_raw, &d_f, &s_f);

    /* Wynik (stabilny do UI) */
    out.distance    = d_f;
    out.strength    = s_f;
    out.temperature = temp_c;
    out.frameReady  = 1u;
    return out;
}

/* ---------- API publiczne: offsety P/L po filtrze ---------- */
TF_LunaData_t TF_Luna_Right_Read(void)
{
    TF_LunaData_t d = TF_Luna_Read_Generic(luna_right, &filt_R);
    int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_right_mm;
    if (v < 0) { v = 0; }
    if (v > 65535) { v = 65535; }
    d.distance = (uint16_t)v;
    return d;
}
TF_LunaData_t TF_Luna_Left_Read(void)
{
    TF_LunaData_t d = TF_Luna_Read_Generic(luna_left, &filt_L);
    int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_left_mm;
    if (v < 0) { v = 0; }
    if (v > 65535) { v = 65535; }
    d.distance = (uint16_t)v;
    return d;
}
