#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "app.h"

#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* moduły projektu */
#include "tf_luna_i2c.h"
#include "tcs3472.h"
#include "ssd1306.h"
#include "oled_panel.h"
#include "debug_uart.h"
#include "i2c_scan.h"
#include "config.h"
#include "motor_bldc.h"
#include "tank_drive.h"
#include "drive_test.h"

/* Okresy zadań (ms) – identycznie jak dotąd */
#define PERIOD_SENS_MS    100U
#define PERIOD_OLED_MS    200U
#define PERIOD_UART_MS    200U

/* Bufory na ostatnie dobre pomiary */
static TF_LunaData_t   g_RightLuna = {0};
static TF_LunaData_t   g_LeftLuna  = {0};
static TCS3472_Data_t  g_RightColor= {0};
static TCS3472_Data_t  g_LeftColor = {0};

/* Zegary odświeżania */
static uint32_t tTank = 0;
static uint32_t tSens = 0;
static uint32_t tOLED = 0;
static uint32_t tUART = 0;

void App_Init(void)
{
    /* 1) UART + skan I2C */
    DebugUART_Init(&huart2);
    DebugUART_Printf("\r\n=== DzikiBoT – start (clean) ===");
    DebugUART_Printf("UART ready @115200 8N1");
    // Przeskanuj obie magistrale I²C — szybka diagnostyka adresów
    I2C_Scan_All();            /* korzysta z hi2c1 i hi2c3 */

    /* 2) Sensory + OLED */
    TF_Luna_Right_Init(&hi2c1);  /* Right (I2C1) */
    TF_Luna_Left_Init (&hi2c3);  /* Left  (I2C3) */

    TCS3472_Right_Init(&hi2c1);  /* Right (I2C1) */
    TCS3472_Left_Init (&hi2c3);  /* Left  (I2C3) */

    SSD1306_Init();
    DebugUART_Printf("SSD1306 init OK.");

    /* 3) ESC + TankDrive */
    ESC_Init(&htim1);
    // 3 s neutralu: pewne „arming” ESC po starcie
    ESC_ArmNeutral(3000);         /* 3 s neutral – tak jak miałeś */
    Tank_Init(&htim1);
    DriveTest_Start();

    DebugUART_Printf("ESC + TankDrive ready.");

    /* 4) Pierwsze odczyty – zasianie struktur */
    // Pierwszy odczyt — zasiej strukturę prawą TF-Luna
    g_RightLuna = TF_Luna_Right_Read();
    // Pierwszy odczyt — zasiej strukturę lewą TF-Luna
    g_LeftLuna  = TF_Luna_Left_Read();
    g_RightColor= TCS3472_Right_Read();
    g_LeftColor = TCS3472_Left_Read();

    /* 5) start zegarów (dokładnie jak miałeś) */
    tTank = tSens = tOLED = tUART = HAL_GetTick();
}

void App_Tick(void)
{
    const uint32_t now = HAL_GetTick();

    /* 1) Tank – rampa/wygładzenie/kompensacja + test (co tick_ms z config) */
    // Rytm napędu: co tick_ms
    if ((now - tTank) >= CFG_Motors()->tick_ms) {
        Tank_Update();
        // Krok testu jazdy (sekcja FWD/NEU/REV) — opcjonalne
        DriveTest_Tick();     /* test krokowy w tle */
        tTank = now;
    }

    /* 2) Sensory (TF-Luna + TCS3472) */
    if ((now - tSens) >= PERIOD_SENS_MS) {

        TF_LunaData_t r = TF_Luna_Right_Read();
        TF_LunaData_t l = TF_Luna_Left_Read();
        if (r.frameReady) g_RightLuna = r;
        if (l.frameReady) g_LeftLuna  = l;

        g_RightColor = TCS3472_Right_Read();
        g_LeftColor  = TCS3472_Left_Read();

        tSens = now;
    }

    /* 3) OLED: panel 7 linii */
    if ((now - tOLED) >= PERIOD_OLED_MS) {
        OLED_Panel_ShowSensors(&g_RightLuna, &g_LeftLuna,
                               &g_RightColor, &g_LeftColor);
        tOLED = now;
    }

    /* 4) UART: panel ANSI „w miejscu” */
    if ((now - tUART) >= PERIOD_UART_MS) {
        DebugUART_SensorsDual(&g_RightLuna, &g_LeftLuna,
                              &g_RightColor, &g_LeftColor);
        tUART = now;
    }

    /* 5) tu w przyszłości AI Sumo:
       AI_Sumo_Update(...);
    */
}
