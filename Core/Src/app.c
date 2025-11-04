/*
 * ============================================================================
 *  MODULE: app — warstwa aplikacyjna (inicjalizacja + harmonogram)
 * -----------------------------------------------------------------------------
 *  CO:
 *    - App_Init(): inicjalizacja peryferiów/modułów + arming ESC.
 *    - App_Tick(): nieblokująca pętla zadań (napęd, sensory, OLED, UART).
 *
 *  ZAŁOŻENIA:
 *    - main.c minimalny: wywołuje tylko App_Init/App_Tick.
 *    - Brak HAL_Delay w App_Tick; jedyny HAL_Delay to ESC_ArmNeutral(3000).
 *    - TIM1: CH1=PA8 (Right), CH4=PA11 (Left).
 *    - Interwały PERIOD_* z config.c (utrzymujemy stare nazwy makr).
 *    - Odczyty sensorów rozfazowane Right⇄Left (mniejsze szczyty na I²C).
 *    - Jitter Tank mierzony i drukowany „po UART” w takcie panelu.
 * ============================================================================
 */

#include "app.h"
#include "config.h"
#include "stm32l4xx_hal.h"
#include "i2c.h"
#include "usart.h"
#include "tim.h"
#include "tf_luna_i2c.h"
#include "tcs3472.h"
#include "ssd1306.h"
#include "oled_panel.h"
#include "debug_uart.h"
#include "i2c_scan.h"
#include "motor_bldc.h"
#include "tank_drive.h"
#include "drive_test.h"
#include <stdbool.h>

/* Okresy (źródło: config.c) */
#ifndef PERIOD_SENS_MS
#  define PERIOD_SENS_MS  (CFG_Scheduler()->sens_ms)
#endif
#ifndef PERIOD_OLED_MS
#  define PERIOD_OLED_MS  (CFG_Scheduler()->oled_ms)
#endif
#ifndef PERIOD_UART_MS
#  define PERIOD_UART_MS  (CFG_Scheduler()->uart_ms)
#endif

/* Bufory danych sensorów */
static TF_LunaData_t   g_RightLuna = {0};
static TF_LunaData_t   g_LeftLuna  = {0};
static TCS3472_Data_t  g_RightColor= {0};
static TCS3472_Data_t  g_LeftColor = {0};

/* Cache konfiguracji */
static const ConfigMotors_t    *g_MotorsCfg = NULL;
static const ConfigScheduler_t *g_SchedCfg  = NULL;

/* Soft-timery */
static uint32_t tTank = 0, tSens = 0, tOLED = 0, tUART = 0;

/* Rozfazowanie: 0=Right, 1=Left */
static uint8_t  s_sensPhase = 0;

/* Jitter Tank — zbierany między kolejnymi printami UART */
static uint32_t s_lastTankExec = 0;
static uint32_t s_jMin = 0xFFFFFFFF;
static uint32_t s_jMax = 0;
static uint64_t s_jSum = 0;
static uint32_t s_jCnt = 0;

/* Harmonogram: anti-drift (trzymamy fazę) */
static inline bool App_TaskDue(uint32_t now, uint32_t *last, uint32_t period)
{
    if (period == 0U) { *last = now; return true; }   // „zawsze”
    const uint32_t elapsed = (uint32_t)(now - *last); // wrap-safe
    if (elapsed >= period) {
        *last += period * (elapsed / period);         // przeskok o wielokrotność
        return true;
    }
    return false;
}
static inline void App_TaskPrime(uint32_t now, uint32_t *last, uint32_t period)
{
    *last = (period == 0U) ? now : (now - period);    // start „od razu”
}

/* ==== Init systemu i modułów ==== */
void App_Init(void)
{
    g_MotorsCfg = CFG_Motors();            // cache wskaźników
    g_SchedCfg  = CFG_Scheduler();

    DebugUART_Init(&huart2);
    DebugUART_Printf("\r\n=== DzikiBoT – start (clean) ===");
    DebugUART_Printf("UART ready @115200 8N1");
    I2C_Scan_All();                        // szybka diagnostyka I²C

    TF_Luna_Right_Init(&hi2c1);            // Right (I2C1)
    TF_Luna_Left_Init (&hi2c3);            // Left  (I2C3)
    TCS3472_Right_Init(&hi2c1);            // Right (I2C1)
    TCS3472_Left_Init (&hi2c3);            // Left  (I2C3)

    SSD1306_Init();
    DebugUART_Printf("SSD1306 init OK.");

    ESC_Init(&htim1);                      // TIM1: CH1=PA8 (Right), CH4=PA11 (Left)
    ESC_ArmNeutral(3000);                  // wymaganie ESC (neutral ~3 s)
    Tank_Init(&htim1);                     // rampa + mapowanie %→µs

    DriveTest_Start();                     // nieblokujący test jazdy

    /* pierwsze dane do OLED/UART „na start” */
    g_RightLuna  = TF_Luna_Right_Read();
    g_LeftLuna   = TF_Luna_Left_Read();
    g_RightColor = TCS3472_Right_Read();
    g_LeftColor  = TCS3472_Left_Read();

    /* prime soft-timerów */
    const uint32_t now = HAL_GetTick();
    App_TaskPrime(now, &tTank, g_MotorsCfg->tick_ms);
    App_TaskPrime(now, &tSens, g_SchedCfg->sens_ms);
    App_TaskPrime(now, &tOLED, g_SchedCfg->oled_ms);
    App_TaskPrime(now, &tUART, g_SchedCfg->uart_ms);

    /* reset zmiennych pomocniczych */
    s_sensPhase    = 0u;
    s_lastTankExec = 0u; s_jMin = 0xFFFFFFFFu; s_jMax = 0u; s_jSum = 0u; s_jCnt = 0u;
}

/* ==== Pętla zadań (nieblokująca) ==== */
void App_Tick(void)
{
    const uint32_t now = HAL_GetTick();
    if (!g_MotorsCfg || !g_SchedCfg) return; // guard

    /* 1) Napęd — rampa + reverse-gate */
    if (App_TaskDue(now, &tTank, g_MotorsCfg->tick_ms)) {

        /* pomiar jittera interwału między wywołaniami Tank_Update() */
        if (s_lastTankExec != 0u) {
            const uint32_t dt = (uint32_t)(now - s_lastTankExec);
            if (dt < s_jMin) s_jMin = dt;
            if (dt > s_jMax) s_jMax = dt;
            s_jSum += dt; s_jCnt++;
        }
        s_lastTankExec = now;

        DriveTest_Tick();                 // nieblokujący krok testu jazdy
        Tank_Update();                    // rampa + mapowanie %→µs
    }

    /* 2) Sensory — rozfazowane Right ⇄ Left (mniejsze szczyty I²C) */
    if (App_TaskDue(now, &tSens, g_SchedCfg->sens_ms)) {
        if (s_sensPhase == 0u) {
            g_RightLuna  = TF_Luna_Right_Read();   // I2C1: TF-Luna Right
            g_RightColor = TCS3472_Right_Read();   // I2C1: TCS Right
            s_sensPhase  = 1u;
        } else {
            g_LeftLuna   = TF_Luna_Left_Read();    // I2C3: TF-Luna Left
            g_LeftColor  = TCS3472_Left_Read();    // I2C3: TCS Left
            s_sensPhase  = 0u;
        }
    }

    /* 3) OLED — panel 7 linii */
    if (App_TaskDue(now, &tOLED, g_SchedCfg->oled_ms)) {
        OLED_Panel_ShowSensors(&g_RightLuna, &g_LeftLuna, &g_RightColor, &g_LeftColor);
    }

    /* 4) UART — panel + JIT linia (druk „po UART”, w tym samym takcie) */
    if (App_TaskDue(now, &tUART, g_SchedCfg->uart_ms)) {
        DebugUART_SensorsDual(&g_RightLuna, &g_LeftLuna, &g_RightColor, &g_LeftColor);

        if (s_jCnt > 0u) {
            const uint32_t avg = (uint32_t)(s_jSum / s_jCnt);
            DebugUART_PrintJitter(g_MotorsCfg->tick_ms, s_jMin, avg, s_jMax, 1u);
        } else {
            DebugUART_PrintJitter(g_MotorsCfg->tick_ms, 0u, 0u, 0u, 0u);
        }
        /* wyczyść okno statystyk do następnego cyklu UART */
        s_jMin = 0xFFFFFFFFu; s_jMax = 0u; s_jSum = 0u; s_jCnt = 0u;
    }
}
