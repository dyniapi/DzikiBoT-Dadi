/**
 * ============================================================================
 *  MODULE: tf_luna_i2c — minimalna obsługa TF-Luna (I²C)
 *  ----------------------------------------------------------------------------
 *  CO:
 *    • Czytamy tylko rejestry 0x00..0x05 (I²C): DIST_L/H, AMP_L/H, TEMP_L/H.
 *    • Temperatura w I²C jest w setnych °C → tempC = (int16_t(TEMP) / 100.0f).
 *    • Filtry: MED (distance) + MA (strength) wg okien z config.c (1..5; MED nieparzyste).
 *    • Zwracamy °C zaokrąglone do 0.1°C (bez <math.h>).
 *
 *  PO CO:
 *    • Prościej, krócej, czytelniej — zero nieużywanych ścieżek (burst/CRC/auto-detect/reset).
 * ============================================================================
 */

#include "tf_luna_i2c.h"     // API i typy modułu
#include "config.h"          // CFG_Luna(): median_win, ma_win, temp_scale, temp_offset_c
#include <string.h>          // memset
#include "stm32l4xx_hal.h"   // HAL I2C, HAL_Delay (krótka przerwa między próbami)

/* ───────────── Adres i time-outy I²C ───────────── */
#define TFLUNA_ADDR            (0x10u << 1)   /* 7-bit 0x10 → 8-bit dla HAL */
#define TFLUNA_TRIES           3u             /* ile prób odczytu rejestrów */
#define TFLUNA_TO_TX           10u            /* timeout TX (ms)            */
#define TFLUNA_TO_RX           10u            /* timeout RX (ms)            */

/* ───────────── Uchwyt I²C dla prawego/lewego czujnika ───────────── */
static I2C_HandleTypeDef *luna_right = NULL;  /* I2C1: prawa TF-Luna  */
static I2C_HandleTypeDef *luna_left  = NULL;  /* I2C3: lewa  TF-Luna  */

void TF_Luna_Right_Init(I2C_HandleTypeDef *hi2c1) { luna_right = hi2c1; }  /* zapamiętaj I²C1 */
void TF_Luna_Left_Init (I2C_HandleTypeDef *hi2c3) { luna_left  = hi2c3; }  /* zapamiętaj I²C3 */

/* ───────────── Proste filtry: MED/MA w buforze pierścieniowym ───────────── */
#define WIN_MAX 5u

typedef struct {
    uint16_t dist_hist[WIN_MAX];  /* historia dystansu (dla mediany)            */
    uint16_t str_hist [WIN_MAX];  /* historia siły (dla średniej)               */
    uint8_t  count;               /* ile wpisów mamy (<= WIN_MAX)               */
    uint8_t  idx;                 /* indeks do nadpisania (ring buffer)         */
    float    last_tempC;          /* ostatnia dobra temperatura (°C)            */
    uint16_t last_med;            /* ostatnia mediana dystansu (cm)             */
    uint16_t last_ma;             /* ostatnia średnia siły (raw)                */
} tfluna_filt_t;

static tfluna_filt_t filt_right = {0};  /* stan filtrów: prawy czujnik */
static tfluna_filt_t filt_left  = {0};  /* stan filtrów: lewy  czujnik */

/* ───────────── Pomocnicze: zaokrąglenie do 0.1°C ───────────── */
static float round_01(float v)
{
    /* Dodaj 0.5 lub −0.5 w skali 0.1 i rzutuj na int → szybkie zaokrąglenie */
    int s = (v >= 0.0f) ? +1 : -1;
    int t = (int)(v * 10.0f + s * 0.5f);
    return (float)t / 10.0f;
}

/* ───────────── Proste MED/MA (bez <math.h>) ───────────── */
static uint16_t median_u16(const uint16_t *arr, uint8_t n)
{
    uint16_t tmp[WIN_MAX];                   /* lokalna kopia do sortowania           */
    for (uint8_t i = 0; i < n; ++i) tmp[i] = arr[i];
    for (uint8_t i = 1; i < n; ++i) {        /* sortowanie wstawieniowe               */
        uint16_t key = tmp[i];
        int j = (int)i - 1;
        while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];                       /* element środkowy                      */
}

static uint16_t mean_u16(const uint16_t *arr, uint8_t n)
{
    uint32_t s = 0;
    for (uint8_t i = 0; i < n; ++i) s += arr[i];
    return (uint16_t)(s / (uint32_t)n);      /* średnia całkowitoliczbowa             */
}

/* ───────────── Aktualizacja filtrów wg okien z config.c ─────────────
 *  MED: wymuszamy okno nieparzyste; oba okna CLAMP do 1..WIN_MAX (1..5).
 */
static void filt_update_cfg(tfluna_filt_t *f, uint16_t dist, uint16_t str,
                            uint16_t *out_med, uint16_t *out_ma)
{
    const ConfigLuna_t *L = CFG_Luna();      /* pobierz okna filtrów                   */

    uint8_t wmed = L->median_win;            /* okno mediany                           */
    if (wmed < 1u) wmed = 1u;
    if (wmed > WIN_MAX) wmed = WIN_MAX;
    if ((wmed & 1u) == 0u) wmed--;           /* mediana wymaga okna nieparzystego      */

    uint8_t wma = L->ma_win;                 /* okno średniej kroczącej                */
    if (wma < 1u) wma = 1u;
    if (wma > WIN_MAX) wma = WIN_MAX;

    f->dist_hist[f->idx] = dist;             /* wpisz nowe wartości do buforów         */
    f->str_hist [f->idx] = str;

    if (f->count < WIN_MAX) f->count++;      /* zwiększ licznik do maks. okna          */
    f->idx = (uint8_t)((f->idx + 1u) % WIN_MAX); /* pierścieniowo                      */

    uint8_t nmed = (f->count < wmed) ? f->count : wmed; /* realna liczba próbek */
    uint8_t nma  = (f->count < wma ) ? f->count : wma;

    uint16_t med = median_u16(f->dist_hist, nmed);      /* policz medianę       */
    uint16_t ma  = mean_u16  (f->str_hist,  nma );      /* policz średnią       */

    f->last_med = med;                        /* zachowaj ostatnie wartości filtrów     */
    f->last_ma  = ma;

    if (out_med) *out_med = med;              /* oddaj wynik do struktury wyjściowej    */
    if (out_ma)  *out_ma  = ma;
}

/* ───────────── Odczyt rejestrowy 0x00..0x05 (1 próba) ───────────── */
static uint8_t tfluna_read_regs_once(I2C_HandleTypeDef *hi2c, TF_LunaData_t *out, tfluna_filt_t *fs)
{
    if (!hi2c || !out || !fs) return 0u;     /* zabezpieczenie argumentów              */

    uint8_t reg  = 0x00u;                    /* start od 0x00                          */
    uint8_t data[6] = {0};                   /* 0..5: D_L,D_H,A_L,A_H,T_L,T_H          */

    if (HAL_I2C_Master_Transmit(hi2c, TFLUNA_ADDR, &reg, 1, TFLUNA_TO_TX) != HAL_OK) return 0u;
    if (HAL_I2C_Master_Receive (hi2c, TFLUNA_ADDR, data, sizeof(data), TFLUNA_TO_RX) != HAL_OK) return 0u;

    /* Złóż słowa 16-bit z par bajtów (LOW|HIGH<<8) */
    uint16_t dist     = (uint16_t)(data[0] | (data[1] << 8));  /* dystans [cm]  */
    uint16_t strength = (uint16_t)(data[2] | (data[3] << 8));  /* siła   [raw]  */
    int16_t  traw     = (int16_t)(data[4] | (data[5] << 8));   /* temp [0.01°C] */

    /* Temperatura I²C: setne °C → °C */
    float tC = (float)traw / 100.0f;                           /* np. 2500→25.00 */

    /* Skala i ograniczenie zakresu (opcjonalna kalibracja) */
    tC *= CFG_Luna()->temp_scale;                               /* mnożnik z config */
    if (tC < -40.0f) tC = -40.0f;                               /* clamp bez „logiki” */
    if (tC > 125.0f) tC = 125.0f;

    /* Wpisz surowe do wyjścia */
    out->distance    = dist;
    out->strength    = strength;
    out->temperature = round_01(tC);                            /* 0.1°C bez <math.h> */

    /* Zaktualizuj filtry i wyprowadź ich wynik */
    filt_update_cfg(fs, dist, strength, &out->distance_filt, &out->strength_filt);

    /* Zapamiętaj ostatnią dobrą temp. do ewentualnego fallbacku */
    fs->last_tempC   = out->temperature;

    out->frameReady  = 1u;                                      /* mamy nową ramkę  */
    return 1u;
}

/* ───────────── Główny odczyt z kilkoma próbami ─────────────
 *  Jeśli wszystkie próby padną, zwracamy ostatnie przefiltrowane
 *  wartości i ostatnią temp. (frameReady=0), aby UI nie „skakało”.
 */
static TF_LunaData_t TF_Luna_Read_Generic(I2C_HandleTypeDef *hi2c, tfluna_filt_t *fs)
{
    TF_LunaData_t out;                         /* lokalna struktura wynikowa           */
    memset(&out, 0, sizeof(out));              /* wyzeruj przed użyciem                */
    if (!hi2c || !fs) return out;              /* brak uchwytu → pusta ramka           */

    for (uint8_t i = 0; i < TFLUNA_TRIES; ++i) {
        if (tfluna_read_regs_once(hi2c, &out, fs)) return out;  /* sukces → zwróć  */
        HAL_Delay(2);                                           /* krótka przerwa  */
    }

    /* FAIL: zwróć ostatnie stabilne filtry + ostatnią temp. */
    out.distance_filt = fs->last_med;
    out.strength_filt = fs->last_ma;
    out.temperature   = (fs->last_tempC == 0.0f) ? 25.0f : fs->last_tempC;
    out.frameReady    = 0u;
    return out;
}

/* ───────────── Publiczne API: aliasy prawa/lewa + offsety z config ───────────── */
TF_LunaData_t TF_Luna_Right_Read(void)
{
    TF_LunaData_t d = TF_Luna_Read_Generic(luna_right, &filt_right);

    /* BYŁO:
       int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_right_mm;
       if (v < 0) v = 0; if (v > 65535) v = 65535;
       d.distance = (uint16_t)v;
    */

    int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_right_mm;
    if (v < 0) {
        v = 0;
    }
    if (v > 65535) {
        v = 65535;
    }
    d.distance = (uint16_t)v;
    return d;
}

TF_LunaData_t TF_Luna_Left_Read(void)
{
    TF_LunaData_t d = TF_Luna_Read_Generic(luna_left, &filt_left);

    /* BYŁO:
       int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_left_mm;
       if (v < 0) v = 0; if (v > 65535) v = 65535;
       d.distance = (uint16_t)v;
    */

    int32_t v = (int32_t)d.distance + (int32_t)CFG_Luna()->dist_offset_left_mm;
    if (v < 0) {
        v = 0;
    }
    if (v > 65535) {
        v = 65535;
    }
    d.distance = (uint16_t)v;
    return d;
}

/* ============================================================================
 *  TF_Luna_AmbientEstimateC
 *  ----------------------------------------------------------------------------
 *  CO:
 *    • Szacuje temperaturę otoczenia na bazie temp. układu (d->temperature)
 *      oraz stałej korekty z config: CFG_Luna()->temp_offset_c (np. −15.0 °C).
 *
 *  PO CO:
 *    • Temp. układu jest zwykle wyższa od ambientu (samonagrzewanie). Offset
 *      pozwala wyświetlać „Ambient (est.)” blisko rzeczywistości.
 *
 *  UWAGI:
 *    • Wynik zaokrąglamy do 0.1 °C, zakres obcinamy do −40..125 °C.
 * ============================================================================
 */
float TF_Luna_AmbientEstimateC(const TF_LunaData_t *d)
{
    /* brak danych → zwróć 0.0 (bezpieczna wartość domyślna) */
    if (d == NULL) {
        return 0.0f;
    }

    /* zsumuj temp. układu z offsetem kalibracyjnym z config */
    float t = d->temperature + CFG_Luna()->temp_offset_c;

    /* ogranicz do sensownego zakresu pracy czujnika */
    if (t < -40.0f) {
        t = -40.0f;
    }
    if (t > 125.0f) {
        t = 125.0f;
    }

    /* Zaokrąglenie do 0.1°C bez <math.h> (unikanie warningów → klamry) */
    int sign   = (t >= 0.0f) ? +1 : -1;            /* znak liczby (do poprawnego zaokr.) */
    int tenths = (int)(t * 10.0f + sign * 0.5f);   /* zaokrągl do najbliższej 0.1        */
    return (float)tenths / 10.0f;                  /* wynik w °C z dokładnością 0.1      */
}
