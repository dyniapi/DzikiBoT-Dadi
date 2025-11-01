/**
 * @file    config.c
 * @brief   Centralna konfiguracja projektu DzikiBoT.
 *
 * Ten plik trzyma „wartości startowe” dla modułów:
 *  - SILNIKI / TANK-DRIVE (ESC na TIM1)
 *  - czujnik odległości TF-Luna (I2C)
 *  - czujnik koloru / jasności TCS3472 (I2C)
 *
 * IDEA:
 *  - Nowa osoba w projekcie powinna móc ZMIENIĆ TYLKO TE liczby poniżej
 *    i nie dotykać reszty kodu.
 *  - Każdy parametr ma opis: „co robi” + „kiedy go ruszać”.
 *
 * WAŻNE:
 *  - Wszystkie struktury są `static const` → to są domyślne ustawienia wbudowane
 *    do firmware. Jeśli kiedyś zrobisz „konfigurację z EEPROM / z UART”, to
 *    właśnie z tych wartości możesz startować.
 */

#include "config.h"

/* ============================================================
 *  [1] KONFIGURACJA NAPĘDU / ESC / TANK-DRIVE
 * ============================================================
 *
 * Struktura: ConfigMotors_t
 *
 * Używa jej moduł napędu (np. tank_drive.c), zwykle w postaci:
 *
 *    const ConfigMotors_t *cfg = CFG_Motors();
 *    Tank_Update(..., cfg);
 *
 * Każde pole poniżej reguluje albo:
 *  - „jak często” aktualizujemy napęd,
 *  - „jak szybko” możemy zmieniać moc,
 *  - „jakie są ograniczenia ESC” dla tego robota.
 *
 * Jeśli coś „szarpie” albo ESC się nie uzbraja – zaczynaj od tej sekcji.
 */
static const ConfigMotors_t g_cfg_motors = {
  /* co ile milisekund wywołujesz cykliczną aktualizację napędu
   * (np. Tank_Update). To musi się zgadzać z Twoim timerem / główną pętlą.
   * Typowa wartość: 10–25 ms.
   *
   * ZMIEŃ, JEŚLI:
   *  - chcesz bardziej reaktywny napęd → ustaw 10–15 ms,
   *  - chcesz mniej obciążenia CPU → ustaw 30–50 ms.
   */
  .tick_ms       = 20,     /* Tank_Update co 20 ms */

  /* maksymalny skok mocy na JEDNO wywołanie (w procentach „Twojej skali”).
   * Przykład: 4 oznacza, że z 0% do 100% „Twojej mocy” dojdziesz w ok. 25 krokach.
   * Przy tick_ms=20 ms daje to ~0,5 s pełnej rampy.
   *
   * ZMIEŃ, JEŚLI:
   *  - silnik / ESC „szczeka”, „traci sync”, albo podwozie robi gwałtowne szarpnięcia → ZMNIEJSZ (np. 2)
   *  - robot reaguje za wolno na skręt → ZWIĘKSZ (np. 6–8)
   */
  .ramp_step_pct = 4,      /* 4% / tick → ~200%/s przy 20 ms */

  /* współczynnik wygładzania EMA (0.0–1.0).
   *  - 0.0  → brak wygładzania (nie używać tutaj)
   *  - 0.1  → bardzo gładko, duże opóźnienie
   *  - 0.2  → sensowna równowaga
   *  - 0.5+ → prawie brak filtrowania
   *
   * ZMIEŃ, JEŚLI:
   *  - masz „dudnienie” lub drgania przy małych prędkościach → zwiększ wygładzanie (np. 0.25–0.3)
   *  - potrzebujesz bardzo szybkiej reakcji na torze → zmniejsz (np. 0.1–0.15)
   */
  .smooth_alpha  = 0.20f,  /* łagodne EMA */

  /* mnożnik dla LEWEGO silnika – do użycia przy różnicach mechanicznych,
   * np. jedno koło większe, inna przekładnia, różnice tarcia.
   * 1.00f → bez zmian
   * 0.95f → lewy silnik jeździ o 5% wolniej
   *
   * ZMIEŃ, JEŚLI:
   *  - robot przy jeździe prosto lekko skręca → ustaw 0.95–0.98 po odpowiedniej stronie
   */
  .left_scale    = 1.00f,  /* dostroisz po montażu kół */

  /* to samo co wyżej, ale dla PRAWEGO silnika */
  .right_scale   = 1.00f,

  /* minimalny procent sygnału ESC (w Twojej skali 0–100), przy którym ESC
   * „zaskakuje” i zaczyna faktycznie kręcić kołem.
   * W wielu ESC dla minisumo/RC koło nie ruszy przy 5–10%,
   * dopiero przy 25–35% zaczyna się ruch – stąd 30.
   *
   * ZMIEŃ, JEŚLI:
   *  - Twoje ESC rusza wcześniej → zmniejsz (20–25)
   *  - ESC ma „martwą strefę” i rusza dopiero przy 35–40% → zwiększ (35–40)
   */
  .esc_start_pct = 30,     /* „start” pracy ESC */

  /* maksymalny procent, którego UŻYWASZ w kodzie jako „pełna moc”.
   * Np. tu: 60 oznacza, że Twoje 100% w tank_drive → wyśle do ESC tylko 60%.
   * To jest dobre, gdy:
   *  - ESC przy 100% jest zbyt agresywny,
   *  - chcesz zostawić sobie „zapas” na przyszłość,
   *  - Twoje koła buksują.
   *
   * ZMIEŃ, JEŚLI:
   *  - robot jest za słaby → zwiększ do 70–80
   *  - robot zrywa przyczepność → zmniejsz do 50
   */
  .esc_max_pct   = 60,     /* „nasze 100%” = 60% ESC-PWM */

  /* czas (ms), przez jaki wymuszamy NEUTRAL pomiędzy przejściami
   * przód ↔ tył. Chroni ESC i przekładnię, bo wiele ESC nie lubi
   * natychmiastowego odwracania kierunku.
   *
   * ZMIEŃ, JEŚLI:
   *  - ESC wywala błąd / piszczy przy szybkim FWD→REV → ZWIĘKSZ (700–1000)
   *  - chcesz szybsze odbijanie się w minisumo → ZMNIEJSZ (300–400),
   *    ALE obserwuj zachowanie ESC!
   */
  .neutral_dwell_ms     = 500,  /* 0.5 s twardy neutral przy FWD<->REV */

  /* próg (%), od którego zmiana znaku uznajemy za „prawdziwy” reverse.
   * Przykład: jeśli joystick / AI lekko przejdzie przez 0 (np. -2%), to
   * nie rób od razu sekwencji neutralnej – dopiero gdy przejdziesz
   * powyżej 5% w drugą stronę.
   *
   * ZMIEŃ, JEŚLI:
   *  - masz „pompowanie” między przód/tył przy szumach sterowania → zwiększ (8–10)
   *  - masz bardzo czyste sterowanie i chcesz, żeby reagował szybciej → zmniejsz (2–3)
   */
  .reverse_threshold_pct= 5,    /* zmiana znaku liczy się dopiero powyżej 5% */
};

/* accessor – reszta kodu bierze ustawienia tylko przez tę funkcję */
const ConfigMotors_t* CFG_Motors(void)
{
  return &g_cfg_motors;
}


/* ============================================================
 *  [2] KONFIGURACJA TF-LUNA (czujnik odległości)
 * ============================================================
 *
 * Struktura: ConfigLuna_t
 *
 * Używają jej moduły tf_luna_i2c.c / Twój panel OLED / Twój DebugUART,
 * zwykle tak:
 *
 *    const ConfigLuna_t *cfg = CFG_Luna();
 *    tf_luna_update(i2c, cfg);
 *
 * Parametry poniżej pozwalają:
 *  - ustalić tempo odświeżania,
 *  - przefiltrować skaczące odczyty,
 *  - zignorować „śmieci” za blisko i za daleko,
 *  - skorygować różnicę PRAWY / LEWY czujnik.
 */
static const ConfigLuna_t g_cfg_luna = {
  /* co ile ms robimy pełny „odczyt + filtrowanie + publikację”
   * z czujnika. Ustaw to tak samo, jak częstotliwość, z jaką
   * wyświetlasz dane (np. 100 ms = 10 Hz).
   *
   * ZMIEŃ, JEŚLI:
   *  - chcesz szybciej reagować na przeszkodę → 50 ms
   *  - chcesz mniej obciążać I2C → 150–200 ms
   */
  .update_ms           = 100,    // spójne z tym, co masz w pętli

  /* Długość okna mediany dla dystansu/siły.
   * Medianę używasz, gdy czasem wpada pojedynczy „głupi” pomiar.
   * 5 = bardzo rozsądnie dla minisumo.
   *
   * ZMIEŃ, JEŚLI:
   *  - masz dużo pojedynczych skoków → zwiększ (7, 9)
   *  - chcesz mniejsze opóźnienie → zmniejsz (3)
   */
  .median_win          = 5,      // MED5 na dystans/siłę

  /* Długość okna średniej ruchomej – druga warstwa wygładzania.
   * 5 próbek przy 100 ms → ok. 0,5 s wygładzania.
   *
   * ZMIEŃ, JEŚLI:
   *  - liczby „pływają” na OLED → zwiększ
   *  - chcesz żywszy odczyt → zmniejsz
   */
  .ma_win              = 5,      // MA5 (ruchome uśrednianie)

  /* Jeśli przez tyle ms nie dostaniesz poprawnej ramki z TF-Luna,
   * moduł wyświetla / zgłasza „NO FRAME” – w UI od razu widać, że
   * czujnik „zniknął”.
   *
   * ZMIEŃ, JEŚLI:
   *  - masz długie i zamierzone przerwy w odczycie → zwiększ (1000)
   *  - chcesz ostrzeżenie „od razu” → zmniejsz (200–300)
   */
  .no_frame_timeout_ms = 500,    // po 0.5 s pokaż „NO FRAME”

  /* minimalna akceptowalna „siła” (strength) echa.
   * TF-Luna potrafi zwrócić dystans, ale z bardzo słabą siłą –
   * wtedy lepiej traktować to jako NIEPEWNY odczyt.
   *
   * ZMIEŃ, JEŚLI:
   *  - mierzysz bardzo refleksyjne cele (będzie wysoka siła) → możesz zwiększyć
   *  - mierzysz matowe / czarne / daleko → zmniejsz, np. 20–30
   */
  .strength_min        = 50,     // poniżej – odrzuć/oznacz słaby odczyt

  /* dolna granica sensownych dystansów w mm.
   * Bardzo blisko czujnik potrafi podać „dziwne” rzeczy – tu je obcinasz.
   * 30 mm = 3 cm.
   *
   * ZMIEŃ, JEŚLI:
   *  - montujesz czujnik bardzo nisko i liczysz na 1–2 cm → ustaw 10–15
   */
  .dist_min_mm         = 30,     // ignoruj bardzo bliskie śmieci

  /* górna granica sensownych dystansów w mm.
   * Powyżej tej wartości uznajemy, że to już „nie nasz zakres” (dla minisumo
   * zwykle nie potrzebujesz >2 m).
   *
   * ZMIEŃ, JEŚLI:
   *  - używasz DzikiBoT do dojazdu z daleka → zwiększ do 3000–4000
   */
  .dist_max_mm         = 2000,   // i zbyt dalekie

  /* skala temperatury – TF-Luna podaje temperaturę w surowej jednostce
   * (często 0.1 °C). Jeśli w Twoim driverze wychodzi np. 25.3 przy skali 1.0,
   * zostaw 1.0. Jeśli wychodzi 2.53 → ustaw 10.0.
   */
  .temp_scale          = 1.0f,   // TF-Luna temp = raw * 0.1

  /* offset dystansu dla PRAWEGO czujnika – użyj gdy:
   *  - prawy czujnik jest bardziej cofnięty w obudowie,
   *  - montowałeś je „na różne głębokości”.
   * Dodawane jest to DO odczytu (czyli 10 → +10 mm).
   */
  .dist_offset_right_mm= 0,      // kalibracja na przyszłość

  /* to samo, ale dla LEWEGO czujnika */
  .dist_offset_left_mm = 0
};

/* accessor – reszta kodu bierze ustawienia tylko przez tę funkcję */
const ConfigLuna_t* CFG_Luna(void)
{
  return &g_cfg_luna;
}


/* ============================================================
 *  [3] KONFIGURACJA TCS3472 (czujnik koloru / jasności)
 * ============================================================
 *
 * Struktura: ConfigTCS_t
 *
 * Służy do:
 *  - zsynchronizowania odczytu z resztą systemu,
 *  - ustawienia czasu integracji (czułość),
 *  - ustawienia gainu wzmacniacza,
 *  - zdefiniowania progów „biały/czarny” dla linii / ringu,
 *  - przycięcia wartości RGB do ładnego wyświetlania.
 */
static const ConfigTCS_t g_cfg_tcs = {
  /* co ile ms robimy odczyt TCS.
   * Po co to pole? Żeby łatwo zgrać się z Luną i OLED-em.
   *
   * ZMIEŃ, JEŚLI:
   *  - chcesz trudniejsze/płynniejsze śledzenie linii → 50 ms
   *  - chcesz tylko „od czasu do czasu” status → 200 ms
   */
  .update_ms       = 100,     // żeby iść równo z Luną (możesz zmienić)

  /* czas integracji matrycy światła w TCS w milisekundach.
   * Dłuższy → jaśniejszy, ale wolniej reaguje.
   * Krótszy → ciemniejszy, ale szybciej reaguje.
   *
   * 50 ms to dobry kompromis na start.
   *
   * ZMIEŃ, JEŚLI:
   *  - czujnik pokazuje małe liczby nawet nad białym → zwiększ (100 ms)
   *  - czujnik jest „przepalony” (ciągle max) → zmniejsz (24 ms)
   */
  .atime_ms        = 50.0f,   // kompromis: nie za długie, nie za krótkie

  /* wzmocnienie wewnętrzne TCS: 1x / 4x / 16x / 60x (zależnie od definicji w .h)
   * U Ciebie było TCS_GAIN_16X – to znaczy: średnio jasne warunki.
   *
   * ZMIEŃ, JEŚLI:
   *  - montujesz czujnik daleko od podłoża → większy gain
   *  - montujesz tuż nad jasną taśmą → mniejszy gain
   */
  .gain            = TCS_GAIN_16X,

  /* wszystkie wartości R/G/B dzielisz tym przed wyświetleniem.
   * U Ciebie UI dzieliło /64, więc tu zostaje 64.
   *
   * ZMIEŃ, JEŚLI:
   *  - chcesz inne skalowanie w UART/OLED → ustaw tu, żeby nie rozjechać UI
   */
  .rgb_divisor     = 64,      // jak dotąd dzieliłeś /64

  /* próg „białości” dla kanału CLEAR.
   * Jeśli clear >= clear_white_thr → traktuj jako białe / bardzo jasne.
   * Dobierasz to na ringu (baza biała + czarna krawędź).
   */
  .clear_white_thr = 1200,    // dobierz na ringu po kalibracji

  /* próg „czerni” dla kanału CLEAR.
   * Jeśli clear <= clear_black_thr → traktuj jako czarne / dziura / krawędź.
   */
  .clear_black_thr = 400,     // jw.

  /* ile KOLEJNYCH pomiarów musi potwierdzić zmianę z białego na czarne
   * (albo odwrotnie), żeby kod uznał, że to nie był szum.
   * 3 oznacza: 3 kolejne pomiary (np. 3×100 ms = 300 ms) muszą potwierdzić.
   *
   * ZMIEŃ, JEŚLI:
   *  - masz drgania / mignięcia → zwiększ
   *  - chcesz super-szybką reakcję → zmniejsz (1–2)
   */
  .edge_debounce   = 3,       // trzy próbki, żeby potwierdzić

  /* skale dla poszczególnych kanałów – przydatne, jeśli lewy i prawy
   * czujnik są z innych serii i „czerwony” ma 15% mniej.
   * Wtedy ustawiasz np. 1.15f dla red_scale.
   */
  .red_scale       = 1.00f,   // na wypadek różnic między modułami
  .green_scale     = 1.00f,
  .blue_scale      = 1.00f
};

/* accessor – reszta kodu bierze ustawienia tylko przez tę funkcję */
const ConfigTCS_t* CFG_TCS(void)
{
  return &g_cfg_tcs;
}
