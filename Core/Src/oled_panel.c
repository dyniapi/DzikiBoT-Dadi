#include "oled_panel.h"
#include "ssd1306.h"
#include <stdio.h>   // snprintf
#include <string.h>

/* Prosty, czytelny panel 7 linii:
 * 0: tytuł
 * 1: Dist (TF-Luna)  MED5
 * 2: Strength        MA5
 * 3: Temp [°C]
 * 4: Clear (TCS)     /64
 * 5: RGB Right       /64
 * 6: RGB Left        /64
 */
void OLED_Panel_ShowSensors(const TF_LunaData_t *R,
                            const TF_LunaData_t *L,
                            const TCS3472_Data_t *CR,
                            const TCS3472_Data_t *CL)
{
    if (!R || !L || !CR || !CL) return;

    char line[32];

    SSD1306_Clear();

    // 0: tytuł / nagłówek
    SSD1306_DrawTextAt(0, 0, "DzikiBoT  R(I2C1) | L(I2C3)");

    // 1: dystans (MED5 z drivera TF-Luna)
    snprintf(line, sizeof(line), "D  R:%3ucm  L:%3ucm",
             (unsigned)R->distance_filt, (unsigned)L->distance_filt);
    SSD1306_DrawTextAt(1, 0, line);

    // 2: sila sygnalu (MA5)
    snprintf(line, sizeof(line), "S  R:%5u  L:%5u",
             (unsigned)R->strength_filt, (unsigned)L->strength_filt);
    SSD1306_DrawTextAt(2, 0, line);

    // 3: temperatura [°C] – sterownik TF-Luna zwraca już float °C
    snprintf(line, sizeof(line), "T  R:%4.1fC L:%4.1fC",
             R->temperature, L->temperature);
    SSD1306_DrawTextAt(3, 0, line);

    // 4: kanał Clear (jasność) – skrót /64, żeby się mieściło
    snprintf(line, sizeof(line), "C  R:%4u   L:%4u",
             (unsigned)(CR->clear/64U), (unsigned)(CL->clear/64U));
    SSD1306_DrawTextAt(4, 0, line);

    // 5: RGB Right
    snprintf(line, sizeof(line), "RGB R:%3u,%3u,%3u",
             (unsigned)(CR->red/64U),
             (unsigned)(CR->green/64U),
             (unsigned)(CR->blue/64U));
    SSD1306_DrawTextAt(5, 0, line);

    // 6: RGB Left
    snprintf(line, sizeof(line), "RGB L:%3u,%3u,%3u",
             (unsigned)(CL->red/64U),
             (unsigned)(CL->green/64U),
             (unsigned)(CL->blue/64U));
    SSD1306_DrawTextAt(6, 0, line);

    SSD1306_UpdateScreen();
}
