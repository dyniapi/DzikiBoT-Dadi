/**
 * @file    debug_uart.c
 * @brief   Panel UART 115200 8N1 (ANSI) do debugowania „w miejscu”.
 * @date    2025-11-02
 *
 * CO:
 *   - Czyści terminal i rysuje dwukolumnowy panel: RIGHT(I2C1) | LEFT(I2C3).
 *   - Pokazuje status ramki, dystans (MED), siłę (MA), temperaturę modułu (°C)
 *     oraz Ambient (est.) – oszacowanie temp. otoczenia na podstawie offsetu z config.
 *
 * PO CO:
 *   - Szybka diagnostyka bez potrzeby podpinania debuggera; widzisz stan czujników
 *     i temperaturę (moduł + ambient est.) w czasie rzeczywistym.
 *
 * KIEDY:
 *   - Wywołuj DebugUART_SensorsDual() cyklicznie co CFG_Scheduler()->uart_ms.
 *
 * UWAGI:
 *   - Do Ambient (est.) używamy TF_Luna_AmbientEstimateC(&data), która dodaje offset
 *     CFG_Luna()->temp_offset_c i zaokrągla do 0.1°C.
 */

#include "debug_uart.h"       // publiczne API modułu
#include <string.h>           // strlen, memset
#include <stdio.h>            // snprintf, vsnprintf
#include <stdarg.h>           // va_list, va_start, va_end

#include "stm32l4xx_hal.h"    // HAL_UART_Transmit
#include "tf_luna_i2c.h"      // TF_LunaData_t, TF_Luna_AmbientEstimateC
#include "tcs3472.h"          // TCS3472_Data_t (typ danych z czujnika kolorów)

/* ========================== UCHWYT LOKALNY ========================== */
/* Przechowujemy wskaźnik na UART ustawiony w DebugUART_Init().        */
static UART_HandleTypeDef *s_uart = NULL;

/* ============================== API ================================= */

void DebugUART_Init(UART_HandleTypeDef *huart)
{
    /* Zapamiętaj uchwyt UART do późniejszych transmisji. */
    s_uart = huart;
}

void DebugUART_Print(const char *s)
{
    /* Bezpieczeństwo: brak UART lub brak stringa → nic nie rób. */
    if (s_uart == NULL || s == NULL) {
        return;
    }

    /* Wyślij treść… */
    HAL_UART_Transmit(s_uart, (uint8_t*)s, (uint16_t)strlen(s), HAL_MAX_DELAY);

    /* …oraz CRLF na końcu (spójny styl w całym module). */
    const char crlf[2] = "\r\n";
    HAL_UART_Transmit(s_uart, (uint8_t*)crlf, (uint16_t)sizeof(crlf), HAL_MAX_DELAY);
}

void DebugUART_Printf(const char *fmt, ...)
{
    if (s_uart == NULL || fmt == NULL) {
        return;
    }

    char buf[160];                       /* lokalny bufor formatowania */
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    DebugUART_Print(buf);                /* wykorzystaj standardowy Print */
}

/* ============================ ANSI HELPER ============================ */
/* Czyści cały ekran i ustawia kursor na (1,1).                         */
static void term_clear(void)
{
    if (s_uart == NULL) {
        return;
    }
    const char *cmd = "\x1b[2J\x1b[H";
    HAL_UART_Transmit(s_uart, (uint8_t*)cmd, (uint16_t)strlen(cmd), HAL_MAX_DELAY);
}

/* ========================== PANEL DIAGNOSTYCZNY ====================== */
/*
 * Dwukolumnowy panel: RIGHT(I2C1) | LEFT(I2C3)
 *  - Status ramki (OK / NO FRAME).
 *  - DIST (MED), STR (MA).
 *  - TEMP (module °C) oraz Ambient (est.) w °C.
 *
 * Wymagane:
 *  - TF_Luna_AmbientEstimateC() dostępne w tf_luna_i2c.c (prototyp w tf_luna_i2c.h).
 */
void DebugUART_SensorsDual(const TF_LunaData_t *RightLuna,
                           const TF_LunaData_t *LeftLuna,
                           const TCS3472_Data_t *RightColor,
                           const TCS3472_Data_t *LeftColor)
{
    /* Wejścia muszą być poprawne; jeśli nie – nie rysujemy panelu. */
    if (s_uart == NULL || RightLuna == NULL || LeftLuna == NULL ||
        RightColor == NULL || LeftColor == NULL) {
        return;
    }

    /* Czyść terminal i rysuj od zera. */
    term_clear();

    char line[160];
    const char *stR = RightLuna->frameReady ? "OK " : "NO FRAME";
    const char *stL = LeftLuna->frameReady  ? "OK " : "NO FRAME";

    DebugUART_Print("                  DzikiBoT (Parametry Sensorów)");
    DebugUART_Print("-------------------------------+-------------------------------------");
    DebugUART_Print("            RIGHT (I2C1)       |               LEFT (I2C3)");
    DebugUART_Print("-------------------------------+-------------------------------------");

    /* DIST – mediana (distance_filt) */
    (void)snprintf(line, sizeof(line),
                   " Dist:  %4ucm  (%-8s)     | Dist:  %4ucm  (%-8s)",
                   (unsigned)RightLuna->distance_filt, stR,
                   (unsigned)LeftLuna->distance_filt,  stL);
    DebugUART_Print(line);

    /* STR – średnia krocząca (strength_filt) */
    (void)snprintf(line, sizeof(line),
                   " Str : %5u                   | Str : %5u",
                   (unsigned)RightLuna->strength_filt,
                   (unsigned)LeftLuna->strength_filt);
    DebugUART_Print(line);

    /* TEMP (module °C) – bezpośrednio z drivera (już 0.1°C) */
    (void)snprintf(line, sizeof(line),
                   " Temp: %5.1f C                 | Temp: %5.1f C",
                   (double)RightLuna->temperature,
                   (double)LeftLuna->temperature);
    DebugUART_Print(line);

    /* AMBIENT (est.) – oszacowanie na podstawie offsetu z CFG_Luna()->temp_offset_c */
    {
        float ambR = TF_Luna_AmbientEstimateC(RightLuna);
        float ambL = TF_Luna_AmbientEstimateC(LeftLuna);

        (void)snprintf(line, sizeof(line),
                       " Amb.: %5.1f C (est)           | Amb.: %5.1f C (est)",
                       (double)ambR, (double)ambL);
        DebugUART_Print(line);
    }

    DebugUART_Print("-------------------------------+-------------------------------------");

    /* RGB/C (skalowane /64 – spójnie z dotychczasowym UI) */
    {
        unsigned rR = RightColor->red   / 64U;
        unsigned gR = RightColor->green / 64U;
        unsigned bR = RightColor->blue  / 64U;
        unsigned cR = RightColor->clear / 64U;

        unsigned rL = LeftColor->red    / 64U;
        unsigned gL = LeftColor->green  / 64U;
        unsigned bL = LeftColor->blue   / 64U;
        unsigned cL = LeftColor->clear  / 64U;

        (void)snprintf(line, sizeof(line),
                       " R:%4u G:%4u B:%4u C:%5u  | R:%4u G:%4u B:%4u C:%5u",
                       rR, gR, bR, cR, rL, gL, bL, cL);
        DebugUART_Print(line);
    }
}
