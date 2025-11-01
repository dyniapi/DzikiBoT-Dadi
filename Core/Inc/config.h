#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ===================== MOTORS / TANK-DRIVE ===================== */
/* Parametry pracy ESC i logiki rampy. Zmieniasz tu, gdy robot
 * szarpie, jedzie krzywo albo ESC nie chce się uzbroić. */
typedef struct {
    uint16_t tick_ms;             /* Co ile ms wołasz Tank_Update (np. 20 ms). */
    uint8_t  ramp_step_pct;       /* Maks. zmiana mocy na 1 krok (np. 4%). */
    float    smooth_alpha;        /* Wygładzanie EMA 0.0–1.0 (0.2 = gładko). */
    float    left_scale;          /* Korekta lewego koła (1.0 = brak). */
    float    right_scale;         /* Korekta prawego koła (1.0 = brak). */
    uint8_t  esc_start_pct;       /* Od ilu % ESC realnie rusza (np. 30%). */
    uint8_t  esc_max_pct;         /* Ile % ESC uznajemy za „nasze 100%” (np. 60%). */
    uint16_t neutral_dwell_ms;    /* Ile ms trzymamy neutral przy FWD<->REV. */
    uint8_t  reverse_threshold_pct;/* Jak duża zmiana znaku to „prawdziwy” reverse. */
} ConfigMotors_t;

/* Zwraca globalną konfigurację napędu. */
const ConfigMotors_t* CFG_Motors(void);


/* ===================== TF-LUNA (distance) ===================== */
/* Ustawienia częstotliwości odczytu i filtrów. Zmieniasz tu, gdy
 * masz skaczący dystans albo czujnik gubi ramki. */
typedef struct {
    uint16_t update_ms;           /* Co ile ms odczyt z czujnika (np. 100 ms). */
    uint8_t  median_win;          /* Okno mediany do wycinania pojedynczych skoków. */
    uint8_t  ma_win;              /* Okno średniej ruchomej – dodatkowe wygładzanie. */
    uint16_t no_frame_timeout_ms; /* Po ilu ms zgłosić „NO FRAME”. */
    uint16_t strength_min;        /* Min. siła sygnału, niżej = traktuj jako słabe. */
    uint16_t dist_min_mm;         /* Za blisko – odrzuć (mm). */
    uint16_t dist_max_mm;         /* Za daleko – odrzuć (mm). */
    float    temp_scale;          /* Skalowanie temperatury z ramki Luny. */
    int16_t  dist_offset_right_mm;/* Korekta prawego sensora w mm. */
    int16_t  dist_offset_left_mm; /* Korekta lewego sensora w mm. */
} ConfigLuna_t;

/* Zwraca globalną konfigurację TF-Luna. */
const ConfigLuna_t* CFG_Luna(void);


/* ===================== TCS3472 (color / clear) ===================== */

/* Gain z kodu czujnika – tak jak w Twoim config.c */
typedef enum {
    TCS_GAIN_1X  = 0,
    TCS_GAIN_4X  = 1,
    TCS_GAIN_16X = 2,
    TCS_GAIN_60X = 3
} TCS_Gain_t;

/* Ustawienia częstotliwości i progów kolorów. Zmieniasz, gdy
 * kalibrujesz ring albo zmieniasz wysokość czujnika. */
typedef struct {
    uint16_t   update_ms;         /* Co ile ms pobierasz dane TCS (np. 100 ms). */
    float      atime_ms;          /* Czas integracji – dłużej = jaśniej. */
    TCS_Gain_t gain;              /* Wzmocnienie czujnika (np. TCS_GAIN_16X). */
    uint16_t   rgb_divisor;       /* Dzielenie RAW-RGB przed wyświetleniem (np. 64). */
    uint16_t   clear_white_thr;   /* CLEAR >= thr → białe/jasne. */
    uint16_t   clear_black_thr;   /* CLEAR <= thr → czarne/krawędź. */
    uint8_t    edge_debounce;     /* Ile kolejnych pomiarów potwierdza zmianę. */
    float      red_scale;         /* Korekta kanału R (1.0 = brak). */
    float      green_scale;       /* Korekta kanału G. */
    float      blue_scale;        /* Korekta kanału B. */
} ConfigTCS_t;

/* Zwraca globalną konfigurację TCS3472. */
const ConfigTCS_t* CFG_TCS(void);

#endif /* CONFIG_H */
