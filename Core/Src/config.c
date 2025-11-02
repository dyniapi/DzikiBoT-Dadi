/*
 * =============================================================================
 *  DzikiBoT — wartości DOMYŚLNE konfiguracji (jedno źródło prawdy do strojenia)
 * -----------------------------------------------------------------------------
 *  CO TU JEST:
 *    • Zestaw „gałek” (parametrów) dla: napędu (tank drive), TF-Luna, TCS3472, scheduler.
 *    • Każde pole opisane: do czego służy, typowy ZAKRES i JAKI jest efekt zmiany.
 *
 *  PO CO:
 *    • Żeby stroić zachowanie bez dotykania logiki modułów (TankDrive, sensory, OLED/UART).
 *    • Zmiany w tym pliku „przechodzą” przez gettery CFG_*() do całego projektu.
 *
 *  JAK CZYTAĆ I STROIĆ (skrót):
 *    - Zaczynaj od wartości domyślnych (poniżej).
 *    - Zmieniaj jedna rzecz naraz i testuj (OLED/UART pokazują efekt).
 *    - Jeśli coś „pływa” – minimalnie zmień rampę (ramp_step_pct) lub wygładzanie (smooth_alpha).
 *
 *  BEZPIECZEŃSTWO:
 *    • Ten plik nie ma HAL_Delay – zmiany parametrów nie blokują czasu wykonania.
 *    • Jedyny „blokujący” fragment jest w App_Init() → ESC_ArmNeutral(3000) (arming ESC na starcie).
 *
 *  UWAGA:
 *    • To są wartości DOMYŚLNE – możesz je dostroić do konkretnej mechaniki/ESCów/sensorów.
 *    • Logika modułów NIE nadpisuje tych wartości w runtime.
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
 *        Zakres: 0.80..1.20 (typowo 1.00 i 1.00).
 *        Podbij lewy/prawy, jeśli robot „ściąga” na prostej.
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
    .median_win             = 3,      // [próbki] mediana (3) usuwa pojedyncze „piki”
    .ma_win                 = 4,      // [próbki] średnia krocząca (4) wygładza trend
    .temp_scale             = 1.0f,   // [×] skala temperatury (zostaw 1.0, jeśli nie kalibrujesz)
    .dist_offset_right_mm   = 0,      // [mm] korekta offsetu (sensor prawy)
    .dist_offset_left_mm    = 0,      // [mm] korekta offsetu (sensor lewy)
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
