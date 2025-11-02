/*
 * =============================================================================
 *  DzikiBoT — wartości DOMYŚLNE konfiguracji (jedno źródło prawdy do strojenia)
 * -----------------------------------------------------------------------------
 *  CO TU JEST:
 *    • Zestaw „gałek” (parametrów) dla: napędu (tank drive), TF-Luna, TCS3472, scheduler.
 *    • Każde pole opisane: do czego służy, typowy ZAKRES i JAKI jest efekt zmiany.
 *
 *  QUICK REF — typowe zakresy strojenia (skrót przydatny „na gorąco”):
 *  ────────────────────────────────────────────────────────────────────────────
 *  [Motors / TankDrive]
 *    tick_ms                : 10..50 ms   (typ. 20)   ↓mniej = szybciej; ↑więcej = lżej dla CPU
 *    neutral_dwell_ms       : 200..800 ms (typ. 600)  ↑więcej = bezpieczniejszy reverse, wolniej
 *    ramp_step_pct          : 1..10 %/tick(typ. 4)    ↑więcej = zrywniej; ↓mniej = łagodniej
 *    reverse_threshold_pct  : 1..5 %      (typ. 3)    ↑więcej = trudniej „przeskoczyć” przez 0%
 *    smooth_alpha           : 0.10..0.40  (typ. 0.25) ↑więcej→mniej filtruje (żywiej)
 *    left_scale/right_scale : 0.90..1.10× (typ. 1.00) korekta prostoliniowości
 *    esc_start_pct          : 20..40 %    (typ. 30)   ↑więcej = mocniejszy „ciąg od dołu”
 *    esc_max_pct            : 50..80 %    (typ. 60)   ↓mniej = ograniczenie szczytowej mocy
 *
 *  [TF-Luna]
 *    median_win             : 1..7        (typ. 3)    ↑więcej = odporność na piki, wolniej
 *    ma_win                 : 1..8        (typ. 4)    ↑więcej = gładszy trend, większe opóźnienie
 *    temp_scale             : 0.5..2.0    (typ. 1.0)  zwykle 1.0
 *    dist_offset_[LR]_mm    : −200..+200  (typ. 0)    korekta po kalibracji
 *
 *  [TCS3472]
 *    atime_ms               : ~24..154 ms (typ. 100)  ↑więcej = większa czułość, wolniej
 *    gain                   : 1× / 4× / 16× / 60×     start: 16×; zmniejsz przy nasyceniu
 *
 *  [Scheduler]
 *    sens_ms                : 50..200 ms  (typ. 100)  częstotliwość odczytu TF-Luna/TCS
 *    oled_ms                : 100..500 ms (typ. 200)  płynność vs. migotanie
 *    uart_ms                : 100..500 ms (typ. 200)  płynność logów
 *  ────────────────────────────────────────────────────────────────────────────
 *
 *  FAQ STROJENIA (szybkie podpowiedzi):
 *  1) „Szarpie przy starcie”
 *     → Zmniejsz ramp_step_pct (np. 2–3), ustaw smooth_alpha nieco niżej (0.20–0.25),
 *       lekko podnieś esc_start_pct (np. 30→33), by konsekwentnie wychodzić z martwej strefy ESC.
 *
 *  2) „Reverse wchodzi zbyt wolno”
 *     → Zmniejsz neutral_dwell_ms (np. 600→350), oraz delikatnie reverse_threshold_pct (3→2).
 *       Uwaga: zbyt małe wartości mogą zwiększyć ryzyko „oscylacji” przy 0%.
 *
 *  3) „Ściąga na prostej”
 *     → Koryguj left_scale/right_scale w krokach ±0.01 (1%) aż do jazdy na wprost (np. 1.00→1.02).
 *
 *  4) „OLED miga / jest ślamazarny”
 *     → Dla migotania: zwiększ oled_ms (200→250/300). Dla większej płynności: zmniejsz (200→150).
 *       Dodatkowo ogranicz liczbę pełnych czyszczeń ekranu w renderze.
 *
 *  5) „TCS3472 się nasyca (maksymalne wartości)”
 *     → Zmniejsz gain (16×→4×→1×) lub skróć atime_ms (100→50). Przy bardzo ciemnym tle: odwrotnie.
 *
 *  6) „TF-Luna pływa / skacze”
 *     → Zwiększ median_win (3→5) albo ma_win (4→6). Pamiętaj: większe okna → wolniejsza odpowiedź.
 *
 *  7) „Napęd reaguje zbyt ospale”
 *     → Zwiększ ramp_step_pct (4→6), podnieś smooth_alpha (0.25→0.35), rozważ krótsze tick_ms (20→15).
 *       Uwaga: krótszy tick to większy narzut CPU.
 *
 *  8) „ESC nie armują się na starcie”
 *     → W App_Init() zwiększ czas ESC_ArmNeutral (np. 3000→4000 ms). Upewnij się, że ESC_Init
 *       jest wywołane przed armingiem i kanały TIM1 (CH1/CH4) faktycznie generują PWM ~50 Hz.
 *
 *  PO CO:
 *    • Stroisz zachowanie bez dotykania logiki modułów (TankDrive, sensory, OLED/UART).
 *    • Zmiany tutaj „przechodzą” przez gettery CFG_*() do całego projektu.
 *
 *  BEZPIECZEŃSTWO:
 *    • Ten plik nie ma HAL_Delay – zmiany parametrów nie blokują czasu wykonania.
 *    • Jedyny „blokujący” fragment jest w App_Init() → ESC_ArmNeutral(3000) (arming ESC na starcie).
 * =============================================================================
 */

#include "config.h"

/* =============================================================================
 *  NAPĘD / TANK DRIVE / RAMPY  (CFG_Motors)
 * -----------------------------------------------------------------------------
 *  Pola i ich rola:
 *    - tick_ms  [ms]    : okres wywołania pętli napędu (Tank_Update).
 *        Zakres: 10..50 ms (typowo 20 ms = 50 Hz). Mniej = szybciej reaguje, ale więcej pracy CPU.
 *        Więcej = spokojniej, kosztem responsywności.
 *
 *    - neutral_dwell_ms [ms] : czas NEUTRAL przy zmianie kierunku (reverse-gate).
 *        Zakres: 200..800 ms (typowo 600 ms). Więcej = bezpieczniej dla napędu, ale wolniejsze zmiany.
 *        Mniej = szybsze przełączanie przód/tył, ale większe ryzyko „szarpnięć”.
 *
 *    - ramp_step_pct    [%/tick] : maks. przyrost |mocy| na pojedynczy tick rampy.
 *        Zakres: 1..10 (%/tick), typowo 4.
 *        Więcej = żywiej startuje (krótsza rampa). Mniej = łagodniej (bardziej miękko).
 *
 *    - reverse_threshold_pct [%] : „martwa” strefa wokół zera do wykrywania zmiany kierunku.
 *        Zakres: 1..5 (%), typowo 3.
 *        Więcej = trudniej przełączyć kierunek (odporne na szum sterowania).
 *        Mniej = szybciej uznasz zmianę znaku, ale podatne na „oscylację” przy 0%.
 *
 *    - smooth_alpha [0.0..1.0] : wygładzanie EMA zmian zadanych (0 = brak, 1 = brak wygładzania).
 *        Zakres: 0.10..0.40 (typowo 0.25).
 *        Więcej (→1) = mniejsze wygładzanie (bardziej responsywnie).
 *        Mniej (→0) = mocniejsze wygładzanie (stabilniej, ale „tępo”).
 *
 *    - left_scale, right_scale [krotność] : balans torów; 1.00 = brak korekty.
 *        Zakres: 0.90..1.10 (typowo 1.00/1.00). Podbij, jeśli robot „ściąga” na prostej.
 *
 *    - esc_start_pct, esc_max_pct [%] : okno „użyteczne” ESC – do tego mapuje się 0..100% napędu.
 *        Zakres: start 20..40 (%), max 50..80 (%). Typowo 30..60 (%).
 *        Więcej „start” = mocniejszy moment od dołu (wyjście z martwej strefy ESC).
 *        Mniej „max”   = ograniczenie górnej mocy (przydatne do kontroli trakcji/minisumo).
 * =============================================================================
 */
static const ConfigMotors_t g_motors = {
    .tick_ms               = 20,     // [ms] rytm Tank_Update; 20 → 50 Hz (responsywne, stabilne)
    .neutral_dwell_ms      = 600,    // [ms] czas neutralu przy zmianie kierunku (reverse-gate)
    .ramp_step_pct         = 4,      // [%/tick] krok rampy (większe → żwawiej, mniejsze → łagodniej)
    .reverse_threshold_pct = 3,      // [%] „bramka” zmiany kierunku (eliminuje oscylacje przy 0%)
    .smooth_alpha          = 0.25f,  // [0..1] wygładzanie EMA; 0.25 = umiarkowane filtrowanie
    .left_scale            = 1.00f,  // [×] korekta lewego toru (1.00 = brak korekty)
    .right_scale           = 1.00f,  // [×] korekta prawego toru (1.00 = brak korekty)
    .esc_start_pct         = 30,     // [%] dolna granica okna ESC (wyjście z martwej strefy)
    .esc_max_pct           = 60,     // [%] górna granica „naszego” 100% (łatwiejsza kontrola)
};

/* =============================================================================
 *  TF-LUNA (I²C) — FILTRY I KOREKTY  (CFG_Luna)
 * -----------------------------------------------------------------------------
 *  Pola i ich rola:
 *    - median_win [próbki] : okno mediany (odporność na pojedyncze odczyty „od czapy”).
 *        Zakres: 1..7 (typowo 3). Więcej = lepsza odporność na skoki, ale wolniejsza reakcja.
 *
 *    - ma_win [próbki]     : okno średniej kroczącej (wygładzenie trendu).
 *        Zakres: 1..8 (typowo 4). Więcej = gładszy sygnał, ale większe opóźnienie.
 *
 *    - temp_scale [krotność] : skala temperatury (1.0 = bez zmian).
 *        Zostaw 1.0, chyba że kalibrujesz specyficzny czujnik; zakres 0.5..2.0.
 *
 *    - dist_offset_right_mm / dist_offset_left_mm [mm] : offset dystansu po kalibracji sceny.
 *        Zakres: ±200 mm (typowo 0). Dodatni = odczyt będzie wyglądał na „dalej”.
 * =============================================================================
 */
 static const ConfigLuna_t g_luna = {
     .median_win             = 3,
     .ma_win                 = 4,
     .temp_scale             = 1.0f,
     .temp_offset_c          = -25.0f,   // ~przybliżenie ambientu względem temp. układu
     .dist_offset_right_mm   = 0,
     .dist_offset_left_mm    = 0,
 };

/* =============================================================================
 *  TCS3472 (I²C) — INTEGRACJA I WZMOCNIENIE  (CFG_TCS)
 * -----------------------------------------------------------------------------
 *  Pola i ich rola:
 *    - atime_ms [ms] : czas integracji (czułość vs. odświeżanie).
 *        Zakres: ~24..154 ms (zależnie od sterownika); typowo 100 ms.
 *        Więcej = jaśniejszy sygnał (większa czułość), ale wolniej się aktualizuje i łatwiej o nasycenie.
 *
 *    - gain (TCS_Gain_t) : wzmocnienie (AGAIN): 1× / 4× / 16× / 60×.
 *        Dobór: zacznij od 16×. Jeśli nasyca się (C/R/G/B „przy sufitach”) — zmniejsz (→4× lub 1×).
 *        Jeśli wartości są bardzo małe — zwiększ (→60×).
 * =============================================================================
 */
static const ConfigTCS_t g_tcs = {
    .atime_ms = 100,            // [ms] integracja: dobry punkt startowy
    .gain     = TCS_GAIN_16X,   // [×] wzmocnienie: 16× (uniwersalne)
};

/* =============================================================================
 *  SCHEDULER (okresy zadań)  (CFG_Scheduler)
 * -----------------------------------------------------------------------------
 *  Pola i ich rola:
 *    - sens_ms [ms] : jak często odczytujesz sensory (TF-Luna/TCS) i aktualizujesz bufor danych.
 *        Zakres: 50..200 ms (typowo 100). Mniej = bardziej „na żywo”, więcej = lżej dla I²C/CPU.
 *
 *    - oled_ms [ms] : jak często odświeżasz ekran OLED.
 *        Zakres: 100..500 ms (typowo 200). Zbyt szybko = mruganie/ghosting, zbyt wolno = „leniwy” UI.
 *
 *    - uart_ms [ms] : jak często aktualizujesz panel UART (ANSI „w miejscu”).
 *        Zakres: 100..500 ms (typowo 200). Podobnie jak OLED – kompromis czytelność/ciągłość.
 * =============================================================================
 */
static const ConfigScheduler_t g_sched = {
    .sens_ms = 100,   // [ms] cykl sensorów
    .oled_ms = 200,   // [ms] cykl OLED
    .uart_ms = 200,   // [ms] cykl UART
};

/* =============================================================================
 *  GETTERY CFG_*() — reszta kodu czyta TYLKO przez te funkcje
 *  (zapewnia spójność i łatwy refactor w przyszłości bez dotykania modułów)
 * =============================================================================
 */
const ConfigMotors_t*     CFG_Motors(void)    { return &g_motors; }
const ConfigLuna_t*       CFG_Luna(void)      { return &g_luna;   }
const ConfigTCS_t*        CFG_TCS(void)       { return &g_tcs;    }
const ConfigScheduler_t*  CFG_Scheduler(void) { return &g_sched;  }
