/*
 * ============================================================================
 *  MODULE: app  —  warstwa aplikacyjna (inicjalizacja systemu + harmonogram)
 *  ----------------------------------------------------------------------------
 *  CO:
 *    - App_Init()  : jednorazowy start systemu (inicjalizacja peryferiów i modułów, ARM ESC).
 *    - App_Tick()  : nieblokująca pętla zadań cyklicznych (napęd, sensory, OLED, UART).
 *
 *  PO CO:
 *    - Oddzielenie logiki „co i kiedy” od main.c, który pozostaje minimalny.
 *    - Jedno miejsce sterujące rytmem całej aplikacji (łatwe strojenie i debug).
 *
 *  KIEDY:
 *    - App_Init()  wołane raz po starcie (po HAL initach z CubeMX).
 *    - App_Tick()  wołane w każdej iteracji while(1) — bez opóźnień blokujących.
 *
 *  USTALENIA:
 *    - Nazwy funkcji, struktur i zmiennych — BEZ ZMIAN (spójne z projektem).
 *    - Interwały PERIOD_* pobierane z config.c przez CFG_Scheduler(), ale zachowujemy
 *      stare nazwy PERIOD_SENS_MS / PERIOD_OLED_MS / PERIOD_UART_MS.
 *    - Napęd: TIM1 CH1=PA8 (Right), CH4=PA11 (Left). ARM ESC: 3000 ms neutralu na starcie.
 *
 *  UWAGI DOT. BEZPIECZEŃSTWA:
 *    - Brak HAL_Delay w App_Tick() — wszystko nieblokujące.
 *    - Jedyny HAL_Delay jest w ESC_ArmNeutral(3000) w App_Init() — wymaganie ESC.
 * ============================================================================
 */

#include "app.h"              // prototypy App_Init/App_Tick
#include "config.h"           // CFG_Scheduler(), CFG_Motors() — okresy i rytm napędu

/* HAL i peryferia z projektu */
#include "stm32l4xx_hal.h"    // HAL_GetTick()
#include "i2c.h"              // hi2c1, hi2c3
#include "usart.h"            // huart2
#include "tim.h"              // htim1

/* Moduły projektu (nazwy z Twojego baseline) */
#include "tf_luna_i2c.h"      // TF_Luna_*()
#include "tcs3472.h"          // TCS3472_*()
#include "ssd1306.h"          // SSD1306_Init()
#include "oled_panel.h"       // OLED_Panel_ShowSensors(...)
#include "debug_uart.h"       // DebugUART_*()
#include "i2c_scan.h"         // I2C_Scan_All()
#include "motor_bldc.h"       // ESC_Init(), ESC_ArmNeutral()
#include "tank_drive.h"       // Tank_Init(), Tank_Update()
#include "drive_test.h"       // DriveTest_Start(), DriveTest_Tick()

#include <stdbool.h>

/* ========================================================================== */
/*  Interwały zadań — UTRZYMUJEMY STARE NAZWY MAKRO, ale źródłem jest config. */
/*  Dzięki temu nie dotykamy reszty kodu, a wartości stroimy w config.c.      */
/* ========================================================================== */
#ifndef PERIOD_SENS_MS
#  define PERIOD_SENS_MS  (CFG_Scheduler()->sens_ms)   // odczyt sensorów (TF-Luna/TCS)
#endif
#ifndef PERIOD_OLED_MS
#  define PERIOD_OLED_MS  (CFG_Scheduler()->oled_ms)   // odświeżanie OLED
#endif
#ifndef PERIOD_UART_MS
#  define PERIOD_UART_MS  (CFG_Scheduler()->uart_ms)   // odświeżanie panelu UART
#endif

/* ============================================================================
 *  Stan lokalny modułu app (wyłącznie dla App_Init/App_Tick)
 *  (Trzymamy tu tylko to, co potrzebne do rytmu zadań.)
 * ========================================================================== */

/* Bufory danych sensorów — tak jak było w Twoim main.c */
static TF_LunaData_t   g_RightLuna = {0};   // TF-Luna (Right, I2C1)
static TF_LunaData_t   g_LeftLuna  = {0};   // TF-Luna (Left,  I2C3)
static TCS3472_Data_t  g_RightColor= {0};   // TCS3472 (Right, I2C1)
static TCS3472_Data_t  g_LeftColor = {0};   // TCS3472 (Left,  I2C3)

static const ConfigMotors_t    *g_MotorsCfg = NULL;   // cache CFG_Motors() po App_Init
static const ConfigScheduler_t *g_SchedCfg  = NULL;   // cache CFG_Scheduler() po App_Init

/* Zegary soft-timerów (ms) — znaczniki ostatniego wykonania zadań */
static uint32_t tTank = 0;  // rytm napędu wg CFG_Motors()->tick_ms (rampa, reverse-gate)
static uint32_t tSens = 0;  // co PERIOD_SENS_MS — odczyty TF-Luna i TCS3472
static uint32_t tOLED = 0;  // co PERIOD_OLED_MS — odświeżenie OLED
static uint32_t tUART = 0;  // co PERIOD_UART_MS — odświeżenie panelu UART

/* Narzędzie do harmonogramu: zwraca true, gdy minął okres zadania i uaktualnia znacznik. */
static inline bool App_TaskDue(uint32_t now, uint32_t *last_run_ms, uint32_t period_ms)
{
    if (period_ms == 0U) {                   // zabezpieczenie: „0” traktujemy jako „zawsze”
        *last_run_ms = now;
        return true;
    }

    if ((uint32_t)(now - *last_run_ms) >= period_ms) {
        *last_run_ms = now;
        return true;
    }

    return false;
}

/* ============================================================================
 *  App_Init() — jednorazowa inicjalizacja aplikacji (po initach Cube/HAL)
 * ========================================================================== */
void App_Init(void)
{
    /* 0) Cache konfiguracji — pobieramy raz (stałe struktury w config.c) */
    g_MotorsCfg = CFG_Motors();
    g_SchedCfg  = CFG_Scheduler();

    /* 1) UART + skan I2C — szybka diagnostyka, jak miałeś w baseline */
    DebugUART_Init(&huart2);                           // ustaw printf/ANSI na UART2
    DebugUART_Printf("\r\n=== DzikiBoT – start (clean) ===");
    DebugUART_Printf("UART ready @115200 8N1");
    I2C_Scan_All();                                    // przeskanuj I2C1 i I2C3

    /* 2) Sensory + OLED — inicjalizacja obu TF-Luna i obu TCS3472, a następnie OLED */
    TF_Luna_Right_Init(&hi2c1);                        // Right (I2C1)
    TF_Luna_Left_Init (&hi2c3);                        // Left  (I2C3)
    TCS3472_Right_Init(&hi2c1);                        // Right (I2C1)
    TCS3472_Left_Init (&hi2c3);                        // Left  (I2C3)

    SSD1306_Init();                                    // uruchom wyświetlacz
    DebugUART_Printf("SSD1306 init OK.");

    /* 3) ESC + TankDrive — zgodnie z Twoją kolejnością */
    ESC_Init(&htim1);                                  // kanały: CH1=PA8 (Right), CH4=PA11 (Left)
    ESC_ArmNeutral(3000);                              // ~3 s neutralu na starcie (arming ESC)
    Tank_Init(&htim1);                                 // spięcie logiki napędu z wyjściem PWM

    /* 4) Start testu jazdy (nieblokujący) */
    DriveTest_Start();

    /* 5) Pierwsze odczyty sensorów — żeby OLED/UART miały realne dane od razu */
    g_RightLuna  = TF_Luna_Right_Read();
    g_LeftLuna   = TF_Luna_Left_Read();
    g_RightColor = TCS3472_Right_Read();
    g_LeftColor  = TCS3472_Left_Read();

    /* 6) Zerujemy znaczniki soft-timerów — aby pierwsza iteracja poszła „od razu” */
    const uint32_t now = HAL_GetTick();
    tTank = now;   // pętla napędu
    tSens = now;   // sensory
    tOLED = now;   // OLED
    tUART = now;   // UART
}

/* ============================================================================
 *  App_Tick() — nieblokująca pętla zadań cyklicznych (wołana w każdej iteracji while(1))
 * ========================================================================== */
void App_Tick(void)
{
    /* Pobieramy aktualny czas w ms (różnicowanie now - tX bezpieczne przy overflow) */
    const uint32_t now = HAL_GetTick();

    /* Jeśli App_Init() nie zdążyło zainicjalizować cache konfiguracji — nic nie rób. */
    if ((g_MotorsCfg == NULL) || (g_SchedCfg == NULL)) {
        return;
    }

    /* ===== 1) Pętla napędu — rampa + reverse-gate =====
     * Rytm z CFG_Motors()->tick_ms (np. 20 ms ⇒ 50 Hz).
     */
    if (App_TaskDue(now, &tTank, g_MotorsCfg->tick_ms)) {
        DriveTest_Tick();                                // krok testu jazdy (nieblokujący)
        Tank_Update();                                   // rampa, reverse-gate, mapowanie %→µs, balans torów
    }

    /* ===== 2) Sensory (TF-Luna + TCS3472) =====
     * Rytm z PERIOD_SENS_MS (wartość z CFG_Scheduler()->sens_ms).
     */
    if (App_TaskDue(now, &tSens, g_SchedCfg->sens_ms)) {
        g_RightLuna  = TF_Luna_Right_Read();             // odczyt z prawego TF-Luna (I2C1)
        g_LeftLuna   = TF_Luna_Left_Read();              // odczyt z lewego  TF-Luna (I2C3)
        g_RightColor = TCS3472_Right_Read();             // odczyt z prawego TCS3472 (I2C1)
        g_LeftColor  = TCS3472_Left_Read();              // odczyt z lewego  TCS3472 (I2C3)
    }

    /* ===== 3) OLED — panel 7 linii ===== */
    if (App_TaskDue(now, &tOLED, g_SchedCfg->oled_ms)) {
        OLED_Panel_ShowSensors(&g_RightLuna, &g_LeftLuna,
                               &g_RightColor, &g_LeftColor); // prezentacja na SSD1306
    }

    /* ===== 4) UART — panel w miejscu (ANSI) ===== */
    if (App_TaskDue(now, &tUART, g_SchedCfg->uart_ms)) {
        DebugUART_SensorsDual(&g_RightLuna, &g_LeftLuna,
                              &g_RightColor, &g_LeftColor);  // ten sam układ co OLED
    }
}
