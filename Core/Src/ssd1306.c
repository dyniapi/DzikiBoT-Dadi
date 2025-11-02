/**
 * @file    ssd1306.c
 * @brief   Wyświetlacz SSD1306 i panel diagnostyczny — prezentacja danych.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 *
 * Funkcje w pliku (skrót):
 *   - ssd1306_sendCommand(uint8_t cmd)
 *   - ssd1306_sendCommands(const uint8_t *cmds, uint16_t n)
 *   - ssd1306_sendData(const uint8_t *data, uint16_t n)
 *   - SSD1306_Init(void)
 *   - SSD1306_Clear(void)
 *   - SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
 *   - SSD1306_DrawHLine(uint8_t y, uint8_t x0, uint8_t x1)
 *   - SSD1306_SetContrast(uint8_t value)
 *   - SSD1306_UpdateScreen(void)
 *   - draw_char_6x8(uint8_t x, uint8_t page, char c)
 *   - SSD1306_DrawChar(uint8_t x, uint8_t page, char c)
 *   - SSD1306_DrawText(uint8_t page, const char *text)
 *   - SSD1306_DrawTextAt(uint8_t page, uint8_t x, const char *text)
 *   - OLED_ShowSensors(const TF_LunaData_t *R, const TF_LunaData_t *L,
                      const TCS3472_Data_t *CR, const TCS3472_Data_t *CL)
 */

#include "ssd1306.h"
#include <string.h>
#include <stdio.h>

/* =================== Sekcja: wewnętrzne definicje i bufor ================= */

#define SSD1306_CTRL_CMD   0x00   /* Co=0, D/C#=0 → polecenie */
#define SSD1306_CTRL_DATA  0x40   /* Co=0, D/C#=1 → dane RAM */

static uint8_t s_buffer[SSD1306_BUF_SIZE];  /* 128x64/8 = 1024 bajty */

/* Skrót do uchwytu I2C (split-CubeMX) */
#define I2C_H (&SSD1306_I2C_HANDLE)

/* =================== Sekcja: czcionka ASCII 6x8 ===========================
 * Czcionka: 5 kolumn bitów + 1 kolumna odstępu = 6 px szerokości, 8 px wysokości.
 * Każdy znak (ASCII 0x20..0x7F) to 5 bajtów pionowych kolumn (LSB=top).
 * Ostatnia 6. kolumna zawsze 0x00 (odstęp).
 * Poniżej standardowy zestaw 5x7 dla podstawowego ASCII.
 * ========================================================================== */

static const uint8_t font5x7[] = {
  /* Spacja (0x20) */ 0x00,0x00,0x00,0x00,0x00,
  /* ! */             0x00,0x00,0x5F,0x00,0x00,
  /* " */             0x00,0x07,0x00,0x07,0x00,
  /* # */             0x14,0x7F,0x14,0x7F,0x14,
  /* $ */             0x24,0x2A,0x7F,0x2A,0x12,
  /* % */             0x23,0x13,0x08,0x64,0x62,
  /* & */             0x36,0x49,0x55,0x22,0x50,
  /* ' */             0x00,0x05,0x03,0x00,0x00,
  /* ( */             0x00,0x1C,0x22,0x41,0x00,
  /* ) */             0x00,0x41,0x22,0x1C,0x00,
  /* * */             0x14,0x08,0x3E,0x08,0x14,
  /* + */             0x08,0x08,0x3E,0x08,0x08,
  /* , */             0x00,0x50,0x30,0x00,0x00,
  /* - */             0x08,0x08,0x08,0x08,0x08,
  /* . */             0x00,0x60,0x60,0x00,0x00,
  /* / */             0x20,0x10,0x08,0x04,0x02,
  /* 0 */             0x3E,0x51,0x49,0x45,0x3E,
  /* 1 */             0x00,0x42,0x7F,0x40,0x00,
  /* 2 */             0x42,0x61,0x51,0x49,0x46,
  /* 3 */             0x21,0x41,0x45,0x4B,0x31,
  /* 4 */             0x18,0x14,0x12,0x7F,0x10,
  /* 5 */             0x27,0x45,0x45,0x45,0x39,
  /* 6 */             0x3C,0x4A,0x49,0x49,0x30,
  /* 7 */             0x01,0x71,0x09,0x05,0x03,
  /* 8 */             0x36,0x49,0x49,0x49,0x36,
  /* 9 */             0x06,0x49,0x49,0x29,0x1E,
  /* : */             0x00,0x36,0x36,0x00,0x00,
  /* ; */             0x00,0x56,0x36,0x00,0x00,
  /* < */             0x08,0x14,0x22,0x41,0x00,
  /* = */             0x14,0x14,0x14,0x14,0x14,
  /* > */             0x00,0x41,0x22,0x14,0x08,
  /* ? */             0x02,0x01,0x51,0x09,0x06,
  /* @ */             0x32,0x49,0x79,0x41,0x3E,
  /* A */             0x7E,0x11,0x11,0x11,0x7E,
  /* B */             0x7F,0x49,0x49,0x49,0x36,
  /* C */             0x3E,0x41,0x41,0x41,0x22,
  /* D */             0x7F,0x41,0x41,0x22,0x1C,
  /* E */             0x7F,0x49,0x49,0x49,0x41,
  /* F */             0x7F,0x09,0x09,0x09,0x01,
  /* G */             0x3E,0x41,0x49,0x49,0x7A,
  /* H */             0x7F,0x08,0x08,0x08,0x7F,
  /* I */             0x00,0x41,0x7F,0x41,0x00,
  /* J */             0x20,0x40,0x41,0x3F,0x01,
  /* K */             0x7F,0x08,0x14,0x22,0x41,
  /* L */             0x7F,0x40,0x40,0x40,0x40,
  /* M */             0x7F,0x02,0x0C,0x02,0x7F,
  /* N */             0x7F,0x04,0x08,0x10,0x7F,
  /* O */             0x3E,0x41,0x41,0x41,0x3E,
  /* P */             0x7F,0x09,0x09,0x09,0x06,
  /* Q */             0x3E,0x41,0x51,0x21,0x5E,
  /* R */             0x7F,0x09,0x19,0x29,0x46,
  /* S */             0x46,0x49,0x49,0x49,0x31,
  /* T */             0x01,0x01,0x7F,0x01,0x01,
  /* U */             0x3F,0x40,0x40,0x40,0x3F,
  /* V */             0x1F,0x20,0x40,0x20,0x1F,
  /* W */             0x7F,0x20,0x18,0x20,0x7F,
  /* X */             0x63,0x14,0x08,0x14,0x63,
  /* Y */             0x07,0x08,0x70,0x08,0x07,
  /* Z */             0x61,0x51,0x49,0x45,0x43,
  /* [ */             0x00,0x7F,0x41,0x41,0x00,
  /* \ */             0x02,0x04,0x08,0x10,0x20,
  /* ] */             0x00,0x41,0x41,0x7F,0x00,
  /* ^ */             0x04,0x02,0x01,0x02,0x04,
  /* _ */             0x40,0x40,0x40,0x40,0x40,
  /* ` */             0x00,0x01,0x02,0x00,0x00,
  /* a */             0x20,0x54,0x54,0x54,0x78,
  /* b */             0x7F,0x48,0x44,0x44,0x38,
  /* c */             0x38,0x44,0x44,0x44,0x20,
  /* d */             0x38,0x44,0x44,0x48,0x7F,
  /* e */             0x38,0x54,0x54,0x54,0x18,
  /* f */             0x08,0x7E,0x09,0x01,0x02,
  /* g */             0x0C,0x52,0x52,0x52,0x3E,
  /* h */             0x7F,0x08,0x04,0x04,0x78,
  /* i */             0x00,0x44,0x7D,0x40,0x00,
  /* j */             0x20,0x40,0x44,0x3D,0x00,
  /* k */             0x7F,0x10,0x28,0x44,0x00,
  /* l */             0x00,0x41,0x7F,0x40,0x00,
  /* m */             0x7C,0x04,0x18,0x04,0x78,
  /* n */             0x7C,0x08,0x04,0x04,0x78,
  /* o */             0x38,0x44,0x44,0x44,0x38,
  /* p */             0x7C,0x14,0x14,0x14,0x08,
  /* q */             0x08,0x14,0x14,0x14,0x7C,
  /* r */             0x7C,0x08,0x04,0x04,0x08,
  /* s */             0x48,0x54,0x54,0x54,0x20,
  /* t */             0x04,0x3F,0x44,0x40,0x20,
  /* u */             0x3C,0x40,0x40,0x20,0x7C,
  /* v */             0x1C,0x20,0x40,0x20,0x1C,
  /* w */             0x3C,0x40,0x30,0x40,0x3C,
  /* x */             0x44,0x28,0x10,0x28,0x44,
  /* y */             0x0C,0x50,0x50,0x50,0x3C,
  /* z */             0x44,0x64,0x54,0x4C,0x44,
  /* { */             0x00,0x08,0x36,0x41,0x00,
  /* | */             0x00,0x00,0x7F,0x00,0x00,
  /* } */             0x00,0x41,0x36,0x08,0x00,
  /* ~ */             0x08,0x04,0x08,0x10,0x08
};

/* =================== Sekcja: funkcje niskopoziomowe I2C =================== */

static void ssd1306_sendCommand(uint8_t cmd)
{
    uint8_t buf[2] = { SSD1306_CTRL_CMD, cmd };
    HAL_I2C_Master_Transmit(I2C_H, SSD1306_I2C_ADDR, buf, 2, 100);
}


/*
static void ssd1306_sendCommands(const uint8_t *cmds, uint16_t n)
{
    while (n--) ssd1306_sendCommand(*cmds++);
}
*/

static void ssd1306_sendData(const uint8_t *data, uint16_t n)
{
    /* Wyślij dane z bajtem kontrolnym DATA (0x40).
       Przy większych porcjach warto by było dzielić na bloki (~16-32B).
       Tu wysyłamy w jednym buforze dla prostoty. */
    /* Maksymalny bufor: 1 (ctrl) + 128 (strona) = 129B – OK dla HAL. */
    uint8_t block[1 + 128];
    block[0] = SSD1306_CTRL_DATA;

    while (n > 0) {
        uint16_t chunk = (n > 128) ? 128 : n;
        memcpy(&block[1], data, chunk);
        HAL_I2C_Master_Transmit(I2C_H, SSD1306_I2C_ADDR, block, chunk + 1, 200);
        data += chunk;
        n    -= chunk;
    }
}

/* =================== Sekcja: podstawowe API =============================== */

void SSD1306_Init(void)
{
    /* Standardowa sekwencja inicjalizacji 128x64 (zasilanie z charge pump) */
    ssd1306_sendCommand(0xAE);               // DISPLAY OFF

    ssd1306_sendCommand(0xD5); ssd1306_sendCommand(0x80); // Display clock
    ssd1306_sendCommand(0xA8); ssd1306_sendCommand(0x3F); // Multiplex 1/64
    ssd1306_sendCommand(0xD3); ssd1306_sendCommand(0x00); // Display offset = 0
    ssd1306_sendCommand(0x40 | 0x00);                     // Start line = 0
    ssd1306_sendCommand(0x8D); ssd1306_sendCommand(0x14); // Charge pump ON
    ssd1306_sendCommand(0x20); ssd1306_sendCommand(0x00); // Memory mode: Horizontal
    ssd1306_sendCommand(0xA1);                            // Segment remap: mirror X
    ssd1306_sendCommand(0xC8);                            // COM scan dec (mirror Y)
    ssd1306_sendCommand(0xDA); ssd1306_sendCommand(0x12); // COM pins
    ssd1306_sendCommand(0x81); ssd1306_sendCommand(0x8F); // Contrast
    ssd1306_sendCommand(0xD9); ssd1306_sendCommand(0xF1); // Pre-charge
    ssd1306_sendCommand(0xDB); ssd1306_sendCommand(0x40); // VCOM detect
    ssd1306_sendCommand(0xA4);                            // Display RAM content
    ssd1306_sendCommand(0xA6);                            // Normal display (not inverted)
    ssd1306_sendCommand(0x2E);                            // Deactivate scroll (safety)
    ssd1306_sendCommand(0xAF);                            // DISPLAY ON

    SSD1306_Clear();
    SSD1306_UpdateScreen();
}

void SSD1306_Clear(void)
{
    memset(s_buffer, 0x00, sizeof(s_buffer));
}

/* Rysuje pojedynczy piksel w buforze */
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
    uint16_t idx = (y / 8) * SSD1306_WIDTH + x;
    uint8_t  bit = 1 << (y & 7);
    if (on) s_buffer[idx] |=  bit;
    else    s_buffer[idx] &= ~bit;
}

/* Prosta linia pozioma */
void SSD1306_DrawHLine(uint8_t y, uint8_t x0, uint8_t x1)
{
    if (y >= SSD1306_HEIGHT) return;
    if (x1 < x0) { uint8_t t = x0; x0 = x1; x1 = t; }
    if (x0 >= SSD1306_WIDTH)  return;
    if (x1 >= SSD1306_WIDTH)  x1 = SSD1306_WIDTH - 1;

    for (uint8_t x = x0; x <= x1; x++) {
        SSD1306_DrawPixel(x, y, 1);
    }
}

void SSD1306_SetContrast(uint8_t value)
{
    ssd1306_sendCommand(0x81);
    ssd1306_sendCommand(value);  /* 0..255 */
}

/* Wyślij cały bufor do wyświetlacza */
void SSD1306_UpdateScreen(void)
{
    /* Użyj adresowania kolumn i stron: 0..127, page 0..(PAGES-1) */
    ssd1306_sendCommand(0x21);   // Set Column Address
    ssd1306_sendCommand(0x00);
    ssd1306_sendCommand(SSD1306_WIDTH - 1);

    ssd1306_sendCommand(0x22);   // Set Page Address
    ssd1306_sendCommand(0x00);
    ssd1306_sendCommand(SSD1306_PAGES - 1);

    /* Wyślij cały bufor (I2C_DATA kontrolnie w kawałkach) */
    ssd1306_sendData(s_buffer, sizeof(s_buffer));
}

/* =================== Sekcja: tekst 6x8 ==================================== */

static void draw_char_6x8(uint8_t x, uint8_t page, char c)
{
    if (page >= SSD1306_PAGES) return;
    if (c < 0x20 || c > 0x7E) c = '?';

    uint16_t fontIndex = (uint16_t)(c - 0x20) * 5; /* 5 kolumn */
    uint16_t bufIndex  = (uint16_t)page * SSD1306_WIDTH + x;

    if (x > SSD1306_WIDTH - 6) return; /* znak 6 px szerokości: 5 kolumn + odstęp */

    /* 5 kolumn z tabeli */
    for (uint8_t i = 0; i < 5; i++) {
        s_buffer[bufIndex + i] = font5x7[fontIndex + i];
    }
    /* Odstęp 1 kolumna = 0x00 */
    s_buffer[bufIndex + 5] = 0x00;
}

void SSD1306_DrawChar(uint8_t x, uint8_t page, char c)
{
    draw_char_6x8(x, page, c);
}

void SSD1306_DrawText(uint8_t page, const char *text)
{
    SSD1306_DrawTextAt(page, 0, text);
}

void SSD1306_DrawTextAt(uint8_t page, uint8_t x, const char *text)
{
    if (!text) return;
    uint8_t pos = x;

    while (*text && pos <= (SSD1306_WIDTH - 6)) {
        SSD1306_DrawChar(pos, page, *text++);
        pos += 6; /* 6 px na znak (5 + spacja) */
    }
}

/* =================== Sekcja: panel diagnostyczny DzikiBoT ================= */

/* ----- PANEL OLED: 7 linii (Title + Dist / Str / Temp / Clear / RGB R / RGB L) ----- */
void OLED_ShowSensors(const TF_LunaData_t *R, const TF_LunaData_t *L,
                      const TCS3472_Data_t *CR, const TCS3472_Data_t *CL)
{
    char line[32];

    SSD1306_Clear();

    /* Linia 0: tytuł (kompaktowy) */
    SSD1306_DrawTextAt(0, 0, "     DzikiBoT    ");

    /* Linia 1: dystans [cm] — krótko, żeby się mieściło */
    snprintf(line, sizeof(line), "D  R:%3ucm   L:%3ucm",
             (unsigned)R->distance, (unsigned)L->distance);
    SSD1306_DrawTextAt(1, 0, line);

    /* Linia 2: siła sygnału */
    snprintf(line, sizeof(line), "S  R:%5u   L:%5u",
             (unsigned)R->strength, (unsigned)L->strength);
    SSD1306_DrawTextAt(2, 0, line);

    /* Linia 3: temperatura [°C] – masz włączony float */
    snprintf(line, sizeof(line), "T  R:%4.1fC  L:%4.1fC",
             R->temperature, L->temperature);
    SSD1306_DrawTextAt(3, 0, line);

    /* Linia 4: jasnosc (clear) — /64, by skrócić liczby */
    snprintf(line, sizeof(line), "C  R:%4u    L:%4u",
             (unsigned)(CR->clear / 64U), (unsigned)(CL->clear / 64U));
    SSD1306_DrawTextAt(4, 0, line);

    /* Linia 5: RGB Right (skalowane /64) */
    snprintf(line, sizeof(line), "RGB R:%3u,%3u,%3u",
             (unsigned)(CR->red/64U), (unsigned)(CR->green/64U), (unsigned)(CR->blue/64U));
    SSD1306_DrawTextAt(5, 0, line);

    /* Linia 6: RGB Left (skalowane /64) */
    snprintf(line, sizeof(line), "RGB L:%3u,%3u,%3u",
             (unsigned)(CL->red/64U), (unsigned)(CL->green/64U), (unsigned)(CL->blue/64U));
    SSD1306_DrawTextAt(6, 0, line);

    /* (opcjonalnie) cienka kreska oddzielająca tytuł od danych:
       SSD1306_DrawHLine(7, 0, 127); // y=7 (między page 0 i 1) */

    SSD1306_UpdateScreen();
}
