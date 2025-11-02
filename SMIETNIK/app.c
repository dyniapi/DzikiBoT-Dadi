\
#include "app.h"
#include "config.h"         // dostarcza okresy i parametry
#include "stm32l4xx_hal.h"  // dla HAL_GetTick()

/*
 * Nie wciągamy wszystkich nagłówków modułów, aby nie robić pętli zależności.
 * Deklarujemy tylko to, czego realnie używamy (forward declarations).
 * Jeżeli masz swoje nagłówki – możesz je tu podmienić na #include "..."
 */
void TankDrive_Update(void);               // z tank_drive.c
void OLED_Panel_Update(void);              // z oled_panel.c
void DebugUART_Refresh(void);              // z debug_uart.c
void ESC_Init(void);                       // z motor_bldc.c (warstwa RC PWM)
void ESC_SetNeutralAll(void);              // jw.
void ESC_ArmNeutral(uint32_t neutral_ms);  // jw.

// --- znaczniki czasu zadań (ms) ---
static uint32_t t_tank = 0;  // ostatnie wykonanie pętli jazdy
static uint32_t t_sens = 0;  // ostatni odczyt sensorów (jeśli agregujesz je osobno)
static uint32_t t_oled = 0;  // ostatnie odświeżenie OLED
static uint32_t t_uart = 0;  // ostatnie odświeżenie panelu UART

// [opcjonalnie] jeżeli masz własny agregator sensorów, zadeklaruj go tutaj:
__attribute__((weak)) void Sensors_UpdateAll(void) { /* opcjonalny hook – pusta domyślnie */ }

void App_Init(void)
{
  // 1) Inicjalizacja warstwy ESC/PWM (włącz timery, MOE itd.)
  ESC_Init();                // uruchamia TIM1 i wyjścia na kanały ESC

  // 2) Ustaw stan neutralny na obu kanałach – bezpieczny start
  ESC_SetNeutralAll();       // 1500 µs na lewy i prawy – brak ruchu

  // 3) ARM ESC – trzymamy neutral tyle, ile określono w configu (blokująco i świadomie)
  ESC_ArmNeutral(CFG_Motors()->esc_arm_ms);

  // 4) Zainicjuj znaczniki czasu tak, aby zadania wykonały się "od razu" przy pierwszym App_Tick()
  const uint32_t now = HAL_GetTick();
  t_tank = now;
  t_sens = now;
  t_oled = now;
  t_uart = now;
}

void App_Tick(void)
{
  // Pobierz aktualny czas w ms (uwaga: różnicowanie now - t_x jest bezpieczne na overflow)
  const uint32_t now = HAL_GetTick();

  // 1) Pętla jazdy / rampa – częstotliwość z CFG_Motors()->tick_ms (np. 20 ms ⇒ 50 Hz)
  if ((uint32_t)(now - t_tank) >= CFG_Motors()->tick_ms) {
    t_tank = now;            // zapamiętaj czas ostatniego wykonania
    TankDrive_Update();      // aktualizacja rampy, neutral dwell, mapowanie % → µs w warstwie ESC
  }

  // 2) Odczyt sensorów – jeżeli masz agregator (tu: hook Sensors_UpdateAll)
  if ((uint32_t)(now - t_sens) >= CFG_Scheduler()->sens_ms) {
    t_sens = now;
    Sensors_UpdateAll();     // domyślnie puste; możesz nadpisać w swoim module sensorów
  }

  // 3) OLED – diagnostyka na ekranie
  if ((uint32_t)(now - t_oled) >= CFG_Scheduler()->oled_ms) {
    t_oled = now;
    OLED_Panel_Update();     // rysuje z bufora danych (TF‑Luna/TCS/napęd itp.)
  }

  // 4) UART – panel w miejscu (ANSI), bez rolowania konsoli
  if ((uint32_t)(now - t_uart) >= CFG_Scheduler()->uart_ms) {
    t_uart = now;
    DebugUART_Refresh();     // wypisuje aktualne wartości – spójne z OLED
  }
}
