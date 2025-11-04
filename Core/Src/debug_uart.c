/**
 * @file    debug_uart.c
 * @brief   Debug panel UART (ANSI) — TX nieblokujące (IRQ) + licznik dropów.
 * @date    2025-11-04
 *
 * Założenia:
 *   - Brak użycia HAL_MAX_DELAY, brak blokowania pętli głównej.
 *   - HAL_UART_Transmit_IT + wewnętrzny bufor kołowy TX (ring buffer).
 *   - Kompatybilne API z Twoim core.zip (Init/Print/Printf/SensorsDual).
 *   - Włączone NVIC dla USART2 (lub używanego UARTu).
 *   - W stm32l4xx_it.c: USARTx_IRQHandler wywołuje HAL_UART_IRQHandler(&huartx).
 *   - (NOWE) Nagłówek: "DzikiBoT (Sensors)   UART dropped=X" — X odświeżany co 2 s.
 */

#include "debug_uart.h"     // publiczne API tego modułu
#include <string.h>         // strlen, memset
#include <stdio.h>          // snprintf, vsnprintf
#include <stdarg.h>         // va_list, va_start, va_end

/* ======================== Stan modułu (prywatny) ====================== */

/* Wskaźnik na uchwyt UART przekazany w DebugUART_Init() */
static UART_HandleTypeDef *s_uart = NULL;

/* Bufor kołowy TX — kolejka wysyłanych bajtów (wewnętrzna) */
static uint8_t s_tx_rb[DEBUG_UART_RB_SIZE];  // pamięć bufora
static volatile size_t s_head = 0;           // indeks dopisywania (head)
static volatile size_t s_tail = 0;           // indeks czytania (tail)

/* Flagi/zmienne stanu TX */
static volatile uint8_t s_tx_busy = 0;       // 1 = aktualnie trwa wysyłka (IT)
static size_t s_active_len = 0;              // ile bajtów ma bieżąca porcja (chunk)

/* Licznik “utraconych bajtów” przy przepełnieniu bufora (diagnostyka) */
static volatile uint32_t s_tx_dropped = 0;

/* Krótki cache nagłówka: wartość dropów odświeżana co 2 s, by nie "migała" */
static uint32_t s_drop_cached = 0;           // ostatnia zapamiętana wartość
static uint32_t s_drop_last_ts = 0;          // kiedy ostatnio zaktualizowano cache
#define DEBUG_UART_DROP_REFRESH_MS  (2000u)  // co 2 sekundy odświeżamy wartość

/* Krótkie makra sekcji krytycznej (blokada przerwań na modyfikację head/tail) */
#ifndef ENTER_CRIT
#define ENTER_CRIT()  uint32_t _primask = __get_PRIMASK(); __disable_irq()
#define EXIT_CRIT()   do { if(!_primask) __enable_irq(); } while(0)
#endif

/* Pomocnicze: ile zajęte/ile wolne w kolejce (1 bajt pustki dla rozróżnienia) */
static inline size_t rb_used(void) { return (s_head - s_tail) % DEBUG_UART_RB_SIZE; }
static inline size_t rb_free(void) { return DEBUG_UART_RB_SIZE - 1u - rb_used(); }

/* ================== Rozpoczęcie wysyłki porcji (prywatne) ============= */

/* Startuje wysyłanie kolejnej porcji, jeśli nic nie leci.
 * Porcja = ciągły fragment od tail do końca bufora (żeby nie owijać). */
static void try_kick_tx(void)
{
    if (!s_uart) return;                 // brak uchwytu UART → nic nie robimy
    if (s_tx_busy) return;               // poprzednia porcja jeszcze leci → poczekaj

    size_t head = s_head;                // lokalne kopie (atomowe odczyty)
    size_t tail = s_tail;

    if (head == tail) return;            // kolejka pusta → nie ma czego wysłać

    /* Wyznacz “chunk” jako liniowy fragment do końca bufora */
    size_t chunk = (head > tail) ? (head - tail) : (DEBUG_UART_RB_SIZE - tail);

    s_active_len = chunk;                // zapamiętaj, ile bajtów wysyłamy w tej porcji
    s_tx_busy = 1;                       // oznacz, że TX jest zajęty

    /* Uruchom asynchroniczną transmisję przerwaniami (bez blokowania) */
    HAL_StatusTypeDef st = HAL_UART_Transmit_IT(s_uart, &s_tx_rb[tail], (uint16_t)chunk);
    if (st != HAL_OK)                    // jeśli HAL nie wystartował (np. błąd)
    {
        s_tx_busy = 0;                   // zwolnij busy — spróbujemy później
        s_active_len = 0;                // nic nie poszło
        /* Uwaga: nie przesuwamy tail, bo nic nie wysłano. */
    }
}

/* ============================ Enqueue (priv) ========================== */

/* Wrzuca len bajtów do kolejki TX. Zwraca ile faktycznie dopisano.
 * Jeśli brak miejsca — nadmiar jest odrzucany, a licznik dropów inkrementowany. */
static size_t DebugUART_Write(const void *data, size_t len)
{
    if (!s_uart || !data || len == 0) return 0;   // brak UART/danych → nic do zrobienia

    const uint8_t *p = (const uint8_t*)data;      // rzut na bajty
    size_t written = 0;                            // ile dopisaliśmy

    ENTER_CRIT();                                  // modyfikacja wskaźników wymaga sekcji krytycznej
    while (written < len)                          // dopóki mamy bajty na wejściu
    {
        if (rb_free() == 0) {                      // brak miejsca w buforze?
            s_tx_dropped += (uint32_t)(len - written); // zlicz “utracone” bajty
            break;                                  // przerwij (bez blokowania CPU)
        }
        s_tx_rb[s_head] = p[written];              // zapisz bajt pod head
        s_head = (s_head + 1u) % DEBUG_UART_RB_SIZE; // przesuwamy head (zawijanie)
        written++;                                  // policz zapisany bajt
    }
    EXIT_CRIT();                                   // odblokuj IRQ

    try_kick_tx();                                 // spróbuj wystartować wysyłkę, jeśli stoję
    return written;                                // może być < len przy przepełnieniu
}

/* ============================== API ================================== */

/* Inicjalizacja modułu: zapamiętuje uchwyt, czyści kolejkę/flagę/liczniki. */
void DebugUART_Init(UART_HandleTypeDef *huart)
{
    s_uart = huart;                 // pamiętaj, którego UARTu używamy (np. &huart2)
    ENTER_CRIT();                   // reset stanu w sekcji krytycznej
    s_head = s_tail = 0;            // pusta kolejka TX
    s_tx_busy = 0;                  // brak trwającej wysyłki
    s_active_len = 0;               // brak aktywnej porcji
    s_tx_dropped = 0;               // wyzeruj licznik dropów
    EXIT_CRIT();                    // koniec resetu

    /* Wyzeruj cache nagłówka */
    s_drop_cached = 0;
    s_drop_last_ts = HAL_GetTick();
}

/* Wysyła podany string + CRLF (enqueue, nieblokujące). */
void DebugUART_Print(const char *s)
{
    if (!s) return;                                 // brak tekstu → nic
    (void)DebugUART_Write(s, strlen(s));            // wrzuć treść do kolejki
    (void)DebugUART_Write("\r\n", 2u);              // dopisz CRLF (spójny styl)
}

/* Wysyła sformatowany tekst (printf) + CRLF (enqueue, nieblokujące). */
void DebugUART_Printf(const char *fmt, ...)
{
    if (!fmt) return;                               // brak formatu → nic

    char buf[160];                                  // lokalny bufor formatowania
    va_list ap;                                     // lista argumentów zmiennych
    va_start(ap, fmt);                              // start pobierania argumentów
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);     // sformatuj
    va_end(ap);                                     // koniec pobierania argumentów

    (void)DebugUART_Write(buf, strlen(buf));        // wrzuć sformatowaną treść
    (void)DebugUART_Write("\r\n", 2u);              // CRLF
}

/* Getter liczby bajtów utraconych (przepełnienia kolejki). */
uint32_t DebugUART_Dropped(void)
{
    return s_tx_dropped;                            // prosty odczyt licznika
}

/* ========================= ANSI helper (priv) ======================== */

/* Wyczyść ekran terminala i ustaw kursor na (1,1) — nieblokujące (enqueue). */
static void term_clear(void)
{
    static const char cmd[] = "\x1b[2J\x1b[H";      // ESC[2J (clear) + ESC[H (home)
    (void)DebugUART_Write(cmd, sizeof(cmd) - 1u);   // do kolejki TX
}

/* ======================== Panel dwukolumnowy ========================= */

void DebugUART_SensorsDual(const TF_LunaData_t *RightLuna,
                           const TF_LunaData_t *LeftLuna,
                           const TCS3472_Data_t *RightColor,
                           const TCS3472_Data_t *LeftColor)
{
    if (!s_uart || !RightLuna || !LeftLuna || !RightColor || !LeftColor)
        return;                                     // brak danych/uart → nic nie drukuj

    term_clear();                                   // nowa „rama” panelu

    char line[160];                                 // wspólny bufor linii
    const char *stR = RightLuna->frameReady ? "OK " : "NO FRAME"; // status prawej Luny
    const char *stL = LeftLuna->frameReady  ? "OK " : "NO FRAME"; // status lewej Luny

    /* --- Nagłówek: "DzikiBoT (Sensors)   UART dropped=X" --- */
    /*    X odświeżany co 2 sekundy (cache), by nie zmieniał się przy każdym repaint. */
    {
        const uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - s_drop_last_ts) >= DEBUG_UART_DROP_REFRESH_MS) {
            s_drop_cached  = DebugUART_Dropped();   // pobierz aktualną wartość
            s_drop_last_ts = now;                   // zapamiętaj czas odświeżenia
        }
        (void)snprintf(line, sizeof(line),
                       "DzikiBoT (Sensors)   UART dropped=%lu",
                       (unsigned long)s_drop_cached);
        DebugUART_Print(line);
    }

    DebugUART_Print("-------------------------------+-------------------------------------");
    DebugUART_Print("            RIGHT (I2C1)       |               LEFT (I2C3)");
    DebugUART_Print("-------------------------------+-------------------------------------");

    /* DIST – filtr (mediana) */
    (void)snprintf(line, sizeof(line),
                   " Dist:  %4u cm  (%-8s)    | Dist:  %4u cm  (%-8s)",
                   (unsigned)RightLuna->distance_filt, stR,
                   (unsigned)LeftLuna->distance_filt,  stL);
    DebugUART_Print(line);

    /* STR – EMA/średnia krocząca siły sygnału */
    (void)snprintf(line, sizeof(line),
                   " Str : %5u                   | Str : %5u",
                   (unsigned)RightLuna->strength_filt,
                   (unsigned)LeftLuna->strength_filt);
    DebugUART_Print(line);

    /* TEMP – temperatura modułu (°C) */
    (void)snprintf(line, sizeof(line),
                   " Temp: %5.1f C                 | Temp: %5.1f C",
                   (double)RightLuna->temperature,
                   (double)LeftLuna->temperature);
    DebugUART_Print(line);

    /* AMBIENT (est.) – estymacja otoczenia z drivera TF_Luna */
    {
        float ambR = TF_Luna_AmbientEstimateC(RightLuna);  // estymacja prawej
        float ambL = TF_Luna_AmbientEstimateC(LeftLuna);   // estymacja lewej

        (void)snprintf(line, sizeof(line),
                       " Amb.: %5.1f C (est)           | Amb.: %5.1f C (est)",
                       (double)ambR, (double)ambL);
        DebugUART_Print(line);
    }

    DebugUART_Print("-------------------------------+-------------------------------------");

    /* RGB/C — spójnie z UI (skalowanie /64) */
    {
        unsigned rR = RightColor->red   / 64u;
        unsigned gR = RightColor->green / 64u;
        unsigned bR = RightColor->blue  / 64u;
        unsigned cR = RightColor->clear / 64u;

        unsigned rL = LeftColor->red    / 64u;
        unsigned gL = LeftColor->green  / 64u;
        unsigned bL = LeftColor->blue   / 64u;
        unsigned cL = LeftColor->clear  / 64u;

        (void)snprintf(line, sizeof(line),
                       " R:%4u G:%4u B:%4u C:%5u  | R:%4u G:%4u B:%4u C:%5u",
                       rR, gR, bR, cR, rL, gL, bL, cL);
        DebugUART_Print(line);
    }
}

/* ===================== HAL callback przerwania TX ==================== */

/* Wywoływana przez HAL po zakończeniu wysyłania bieżącej porcji (chunk). */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != s_uart) return;                     // filtr: tylko nasz UART

    ENTER_CRIT();                                    // modyfikujemy tail/flagę
    s_tail = (s_tail + s_active_len) % DEBUG_UART_RB_SIZE; // przesuń ogon o wysłaną porcję
    s_active_len = 0;                                // porcja już wysłana
    s_tx_busy = 0;                                   // TX wolny — można ruszyć następną
    EXIT_CRIT();                                     // koniec sekcji krytycznej

    try_kick_tx();                                   // jeśli są kolejne bajty — start kolejnej porcji
}

/* ====================== NOWE: linia z jitterem ======================= */
/* Proste API do dopisania 1 linii z min/avg/max jittera rytmu Tank.
 * Wołaj po DebugUART_SensorsDual(...) (np. w bloku tUART w App_Tick).
 * Gdy valid==0, drukuje komunikat „zbieram próbki...”.
 */
void DebugUART_PrintJitter(uint32_t tick_ms,
                           uint32_t jMin_ms,
                           uint32_t jAvg_ms,
                           uint32_t jMax_ms,
                           uint8_t  valid)
{

	 DebugUART_Print("-------------------------------+-------------------------------------");
	if (!valid) {
        DebugUART_Printf("     [JIT] Tank tick=%lums  (zbieram próbki...)", (unsigned long)tick_ms);
        return;
    }
    DebugUART_Printf("     [JIT] Tank tick=%lums  min=%lums  avg=%lums  max=%lums",
                     (unsigned long)tick_ms,
                     (unsigned long)jMin_ms,
                     (unsigned long)jAvg_ms,
                     (unsigned long)jMax_ms);
}
