#pragma once
/*
 * ============================================================================
 *  DzikiBoT — globalna konfiguracja runtime (STM32 Nucleo-L432KC)
 *  ----------------------------------------------------------------------------
 *  CO TU JEST:
 *    • Struktury konfiguracyjne dla: napędu (tank drive), TF-Luna, TCS3472 oraz prosty Scheduler.
 *    • Krótkie „typowe zakresy strojenia” przy każdej strukturze (quick ref).
 *    • Gettery CFG_*() zwracające const-wskaźniki na statyczne struktury w config.c.
 *
 *  ZAŁOŻENIA:
 *    • Nie zmieniamy nazw istniejących typów/pól: ConfigMotors_t, ConfigLuna_t, ConfigTCS_t,
 *      ConfigScheduler_t oraz enum TCS_Gain_t.
 *    • Logika modułów odczytuje WYŁĄCZNIE przez CFG_*() — spójność i łatwe strojenie.
 * ============================================================================
 */

#include <stdint.h>

/* ===============================
 *  Napęd / Tank drive / Rampy
 * ===============================
 *
 *  Typowe zakresy strojenia (quick ref):
 *    tick_ms                 : 10..50  ms  (typ. 20)    ↓mniej = szybciej reaguje; ↑więcej = mniejszy narzut CPU
 *    neutral_dwell_ms        : 200..800 ms  (typ. 600)   ↑więcej = bezpieczniej przy reverse (wolniej zmienia kierunek)
 *    ramp_step_pct           : 1..10   %/tick (typ. 4)   ↑więcej = ostrzejszy start/hamowanie; ↓mniej = bardziej miękko
 *    reverse_threshold_pct   : 1..5    %      (typ. 3)   ↑więcej = trudniej „przeskoczyć” przez 0%; ↓mniej = ryzyko oscylacji
 *    smooth_alpha            : 0.10..0.40     (typ. 0.25)↑więcej→mniej filtruje (żywiej); ↓mniej→bardziej stabilnie
 *    left_scale/right_scale  : 0.90..1.10×    (typ. 1.00)korekta prostoliniowości torów
 *    esc_start_pct           : 20..40  %      (typ. 30)  ↑więcej = mocniejszy „ciąg od dołu”
 *    esc_max_pct             : 50..80  %      (typ. 60)  ↓mniej = ograniczenie szczytowej mocy (kontrola trakcji)
 */
typedef struct
{
    uint16_t tick_ms;               /* rytm Tank_Update (ms), np. 20 → 50 Hz                    */
    uint16_t neutral_dwell_ms;      /* czas neutralu przy zmianie kierunku (ms)                */
    uint8_t  ramp_step_pct;         /* maks. przyrost |mocy| na tick [%]                       */
    uint8_t  reverse_threshold_pct; /* martwa strefa wokół 0% do detekcji zmiany znaku [%]     */
    float    smooth_alpha;          /* EMA 0..1 (0=pełne wygładzanie, 1=brak wygładzania)      */
    float    left_scale;            /* kompensacja toru lewego (1.00 = brak korekty)           */
    float    right_scale;           /* kompensacja toru prawego (1.00 = brak korekty)          */
    uint8_t  esc_start_pct;         /* dolna granica „okna ESC” [%] (wyjście z martwej strefy) */
    uint8_t  esc_max_pct;           /* górna granica „okna ESC” [%] (nasz 100%)                */
} ConfigMotors_t;

/* ==========================
 *  TF-Luna (I²C) — filtry
 * ==========================
 *
 *  Typowe zakresy strojenia (quick ref):
 *    median_win              : 1..7   (typ. 3)   ↑więcej = lepsza odporność na pojedyncze piki, wolniej reaguje
 *    ma_win                  : 1..8   (typ. 4)   ↑więcej = gładszy trend, większe opóźnienie
 *    temp_scale              : 0.5..2.0 (typ. 1.0)skalowanie temperatury (zwykle 1.0)
 *    dist_offset_[LR]_mm     : −200..+200 mm (typ. 0)   korekta po kalibracji sceny
 */
typedef struct
{
    uint8_t  median_win;            /* rozmiar okna mediany (redukcja pojedynczych skoków)     */
    uint8_t  ma_win;                /* rozmiar okna średniej kroczącej (wygładzenie trendu)    */
    float    temp_scale;            /* skala temp. (1.0 = bez zmian)                           */

    /* ↓↓↓ NOWE: stała korekta do „ambient est.” (°C), np. -15.0 zmniejszy temp. układu o 15 °C */
    float    temp_offset_c;         /* [°C] offset do szacowania temperatury otoczenia         */

    int16_t  dist_offset_right_mm;  /* offset dystansu [mm] — sensor prawy                     */
    int16_t  dist_offset_left_mm;   /* offset dystansu [mm] — sensor lewy                      */
} ConfigLuna_t;

/* ===========================================
 *  TCS3472 (I²C) — integracja i wzmocnienie
 * ===========================================
 *
 *  Typowe zakresy strojenia (quick ref):
 *    atime_ms                : ~24..154 ms (typ. 100)   ↑więcej = większa czułość, wolniejsza aktualizacja
 *    gain (TCS_Gain_t)       : 1× / 4× / 16× / 60×      zacznij od 16×; zmniejsz przy nasyceniu, zwiększ gdy ciemno
 */
typedef enum
{
    TCS_GAIN_1X  = 0x00,
    TCS_GAIN_4X  = 0x01,
    TCS_GAIN_16X = 0x02,
    TCS_GAIN_60X = 0x03
} TCS_Gain_t;

typedef struct
{
    uint16_t   atime_ms;            /* czas integracji (ms): kompromis czułość/odświeżanie     */
    TCS_Gain_t gain;                /* wzmocnienie AGAIN (patrz enum wyżej)                    */
} ConfigTCS_t;

/* ==========================
 *  Scheduler (okresy zadań)
 * ==========================
 *
 *  Typowe zakresy strojenia (quick ref):
 *    sens_ms                 : 50..200 ms (typ. 100)    częstotliwość odczytów TF-Luna/TCS
 *    oled_ms                 : 100..500 ms (typ. 200)   odświeżanie OLED (balans płynność/migotanie)
 *    uart_ms                 : 100..500 ms (typ. 200)   odświeżanie panelu UART (czytelność/logi)
 */
typedef struct
{
    uint16_t sens_ms;               /* co ile ms odświeżać sensory (TF-Luna/TCS)               */
    uint16_t oled_ms;               /* co ile ms odświeżać OLED                                 */
    uint16_t uart_ms;               /* co ile ms odświeżać panel UART                           */
} ConfigScheduler_t;

/* ====================
 *  GETTERY CFG_*()
 * ==================== */
#ifdef __cplusplus
extern "C" {
#endif

const ConfigMotors_t*     CFG_Motors(void);     /* napęd / rampa / okno ESC / balans torów   */
const ConfigLuna_t*       CFG_Luna(void);       /* TF-Luna — filtry i offsety                 */
const ConfigTCS_t*        CFG_TCS(void);        /* TCS3472 — integracja + gain                */
const ConfigScheduler_t*  CFG_Scheduler(void);  /* Scheduler — okresy zadań                    */

#ifdef __cplusplus
}
#endif
