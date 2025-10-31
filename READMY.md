# DzikiBoT - Dadi (STM32 Nucleo-L432KC)

Ten plik opisuje aktualny stan projektu produkcyjnego DzikiBoT-Dadi.
Projekt wykorzystuje plytke STM32 Nucleo-L432KC oraz szyny I2C do
obslugi czujnikow i wyswietlacza diagnostycznego.

## Sprzet
- MCU: STM32 Nucleo-L432KC
- I2C1 (PB6/PB7) - prawa strona: TF-Luna, TCS3472, OLED SSD1306 (0x3C)
- I2C3 (PC0/PC1) - lewa strona: TF-Luna, TCS3472
- OLED: SSD1306 128x64 - panel 7-liniowy
- Naped: 2x ESC na TIM1
  - TIM1 CH1 = PA8  -> ESC prawy
  - TIM1 CH4 = PA11 -> ESC lewy
  - ARM/neutral ok. 3 s

## Moduly w repo
- tf_luna_i2c.c/.h
- tcs3472.c/.h
- ssd1306.c/.h, oled_panel.c/.h
- debug_uart.c/.h
- motor_bldc.c/.h
- tank_drive.c/.h
- i2c_scan.c/.h
- config.c/.h

## Jak uzyc (Windows 11 + GitHub Desktop)
1. Sklonuj repo dyniapi/DzikiBoT-Dadi
2. Otworz w STM32CubeIDE
3. Zbuduj i wgraj na Nucleo-L432KC
