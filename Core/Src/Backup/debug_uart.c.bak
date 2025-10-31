#include "debug_uart.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>    // prototypy printf/snprintf/vsnprintf
#include <stdarg.h>   // jeśli używasz va_list / vsnprintf (np. w debug_uart.c)


/* ====== prywatny uchwyt ====== */
static UART_HandleTypeDef *s_uart = NULL;

void DebugUART_Init(UART_HandleTypeDef *huart)
{
    s_uart = huart;
}

void DebugUART_Print(const char *s)
{
    if (!s_uart || !s) return;
    HAL_UART_Transmit(s_uart, (uint8_t*)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
    const char crlf[2] = "\r\n";
    HAL_UART_Transmit(s_uart, (uint8_t*)crlf, 2, HAL_MAX_DELAY);
}

void DebugUART_Printf(const char *fmt, ...)
{
    if (!s_uart || !fmt) return;
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    DebugUART_Print(buf);
}

/* Czyść ekran i przejdź do (1,1) */
static void term_clear(void)
{
    const char *cmd = "\x1b[2J\x1b[H";
    HAL_UART_Transmit(s_uart, (uint8_t*)cmd, (uint16_t)strlen(cmd), HAL_MAX_DELAY);
}

/* Dwukolumnowy panel: RIGHT(I2C1) | LEFT(I2C3)
   - pokazuje status ramki (OK/NO FRAME)
   - wartości przefiltrowane (DIST=MED5, STR=MA5), temperatura w °C (float) */


void DebugUART_SensorsDual(const TF_LunaData_t *RightLuna,
                           const TF_LunaData_t *LeftLuna,
                           const TCS3472_Data_t *RightColor,
                           const TCS3472_Data_t *LeftColor)
{
    if (!s_uart || !RightLuna || !LeftLuna || !RightColor || !LeftColor) return;

    term_clear();

    char line[160];
    const char *stR = RightLuna->frameReady ? "OK " : "NO FRAME";
    const char *stL = LeftLuna->frameReady  ? "OK " : "NO FRAME";

    DebugUART_Print("                  DzikiBoT (Parametry Sensorów)");
    DebugUART_Print( "-------------------------------+-------------------------------------");
    DebugUART_Print("            RIGHT (I2C1)       |               LEFT (I2C3)");
    DebugUART_Print( "-------------------------------+-------------------------------------");

    /* DIST – używamy MED5 (distance_filt) */
    snprintf(line, sizeof(line),
             " Dist:  %4ucm  (%-8s)     | Dist:  %4ucm  (%-8s)",
             (unsigned)RightLuna->distance_filt, stR,
             (unsigned)LeftLuna->distance_filt,  stL);
    DebugUART_Print(line);

    /* STR – używamy MA5 (strength_filt) */
    snprintf(line, sizeof(line),
             " Str : %5u                   | Str : %5u",
             (unsigned)RightLuna->strength_filt,
             (unsigned)LeftLuna->strength_filt);
    DebugUART_Print(line);

    /* TEMP – w °C (driver zwraca już float °C) */
    snprintf(line, sizeof(line),
             " Temp: %5.1f C                 | Temp: %5.1f C",
             RightLuna->temperature, LeftLuna->temperature);
    DebugUART_Print(line);

    DebugUART_Print( "-------------------------------+-------------------------------------");

    /* RGB/C (skalowane /64, tak jak wcześniej) */
    {
        unsigned rR = RightColor->red/64U, gR = RightColor->green/64U, bR = RightColor->blue/64U, cR = RightColor->clear/64U;
        unsigned rL = LeftColor->red/64U,  gL = LeftColor->green/64U,  bL = LeftColor->blue/64U,  cL = LeftColor->clear/64U;

        snprintf(line, sizeof(line),
                 " R:%4u G:%4u B:%4u C:%5u  | R:%4u G:%4u B:%4u C:%5u",
                 rR, gR, bR, cR, rL, gL, bL, cL);
        DebugUART_Print(line);
    }
}
