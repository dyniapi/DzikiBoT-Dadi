/* USER CODE BEGIN Header */
/**
 TEST GIT
  ******************************************************************************
  * @file           : main.c
  * @brief          : Główny program DzikiBoT (STM32L432KC)
  *
  * Funkcje:
  *  - Inicjalizacja peryferiów (GPIO, USART2, I2C1/I2C3, TIM1)
  *  - Uruchomienie modułów wysokiego poziomu:
  *      • Debug UART (panel na żywo w terminalu)
  *      • I2C skan (diagnostyka adresów)
  *      • SSD1306 (OLED 128x64 – 7 linii)
  *      • TF-Luna (Right=I2C1, Left=I2C3) – odporne I2C
  *      • TCS3472 (Right=I2C1, Left=I2C3)
  *      • ESC BLDC (TIM1 CH1=PA8 [Right], CH4=PA11 [Left]) + uzbrajanie
  *      • TankDrive – sterowanie dwoma ESC (rampa, nieblokujące)
  *
  * Pętla główna:
  *  - co 100 ms: odczyt sensorów (TF-Luna + TCS3472)
  *  - co 200 ms: rysowanie OLED i panel Debug UART
  *  - co ~tick_ms: Tank_Update() (np. co 20 ms) – rampa i wyjścia PWM
  *
  * Uwaga:
  *  - Kod modułowy: główna logika w plikach .c/.h, a w main.c tylko
  *    minimalna sekwencja inicjalizacji oraz wywołania okresowe.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* ====== Moduły własne (osobne pliki) ===================================== */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


/* --- Moduły projektu (OSOBNE pliki, trzymamy porządek) --- */
#include "tf_luna_i2c.h"   /* TF-Luna (I2C) Right/Left */
#include "tcs3472.h"       /* TCS3472 Right/Left       */
#include "ssd1306.h"       /* Niski driver OLED        */
#include "oled_panel.h"    /* Warstwa prezentacji: panel 7 linii na OLED */
#include "debug_uart.h"    /* ANSI panel po UART       */
#include "i2c_scan.h"      /* Skany I2C (Right/Left)   */

#include "config.h"       /* CFG_Motors(): tick, rampa, kompensacja, okno 30..60% */
#include "motor_bldc.h"   /* ESC_Init, ESC_ArmNeutral, ESC_Write* */
#include "tank_drive.h"   /* Tank_Init, Tank_Update, API manewrów */
#include "drive_test.h"



/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


/* ==== Okresy zadań (ms) – spokojne, czytelne ticki ==== */
#define PERIOD_TANK_MS     20U   /* Tank_Update (rampa)   – co 20 ms */
#define PERIOD_SENS_MS    100U   /* Odczyty sensorów      – co 100 ms */
#define PERIOD_OLED_MS    200U   /* Odświeżanie OLED      – co 200 ms */
#define PERIOD_UART_MS    200U   /* Panel UART            – co 200 ms */


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* Bufory danych czujników (Right=I2C1, Left=I2C3) */
/* ==== Bufory na „ostatnie dobre” pomiary – prosto dostępne w całym main ==== */
static TF_LunaData_t   g_RightLuna = {0};
static TF_LunaData_t   g_LeftLuna  = {0};
static TCS3472_Data_t  g_RightColor= {0};
static TCS3472_Data_t  g_LeftColor = {0};


/* Zegary odświeżania */
static uint32_t tTank = 0;
static uint32_t tSens = 0;
static uint32_t tOLED = 0;
static uint32_t tUART = 0;



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

//int __io_putchar(int ch);  // przekierowanie printf przez UART

/* =========================================================== */
/*                         FUNKCJE                             */
/* =========================================================== */

/**
 * @brief  Przekierowanie funkcji printf do UART2
 *         Umożliwia wyświetlanie danych w terminalu (np. PuTTY)
 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}




/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */


  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */


  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */




  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_I2C3_Init();
  /* USER CODE BEGIN 2 */


  /* 3) Debug UART + I2C scan */
    DebugUART_Init(&huart2);
    DebugUART_Printf("\r\n=== DzikiBoT – start (clean) ===");
    DebugUART_Printf("UART ready @115200 8N1");
    I2C_Scan_All();            /* korzysta z hi2c1 i hi2c3 */

    /* 4) Sensory + OLED */
    TF_Luna_Right_Init(&hi2c1);  /* Right (I2C1) */
    TF_Luna_Left_Init (&hi2c3);  /* Left  (I2C3) */

    TCS3472_Right_Init(&hi2c1);  /* Right (I2C1) */
    TCS3472_Left_Init (&hi2c3);  /* Left  (I2C3) */

    SSD1306_Init();               /* Twój driver: void, bez argumentów */
    DebugUART_Printf("SSD1306 init OK.");

    /* 5) ESC + TankDrive (parametry z config) */
    ESC_Init(&htim1);
    ESC_ArmNeutral(3000);         /* proste, pewne ARM – neutral 1500us przez 3 s */

    Tank_Init(&htim1);            /* rampa/EMA/kompensacja/okno 30..60% → z CFG_Motors() */
    DriveTest_Start();			//test silników








    DebugUART_Printf("ESC + TankDrive ready.");

    /* 6) Pierwsze odczyty – zasianie struktur */
    g_RightLuna = TF_Luna_Right_Read();
    g_LeftLuna  = TF_Luna_Left_Read();
    g_RightColor= TCS3472_Right_Read();
    g_LeftColor = TCS3472_Left_Read();

    /* 7) Pętla główna – tylko okresowe ticki (bez testów) */
 //   const uint16_t tick_ms = CFG_Motors()->tick_ms;  /* np. 20 ms – źródło prawdy */
    tTank = tSens = tOLED = tUART = HAL_GetTick();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  const uint32_t now = HAL_GetTick();


	  /* 7.1) Tank – rampa/wygładzenie/kompensacja + test (co tick_ms z config) */
	  if ((now - tTank) >= CFG_Motors()->tick_ms) {
	    Tank_Update();
	    DriveTest_Tick();     // test krokowy w tle
	    tTank = now;
	  }



	      /* 7.2) Sensory (TF-Luna + TCS3472) */
	      if ((now - tSens) >= PERIOD_SENS_MS) {

	    	TF_LunaData_t r = TF_Luna_Right_Read();
	        TF_LunaData_t l = TF_Luna_Left_Read();
	        if (r.frameReady) g_RightLuna = r;  /* aktualizuj tylko „nową dobrą” ramką */
	        if (l.frameReady) g_LeftLuna  = l;

	        g_RightColor = TCS3472_Right_Read();
	        g_LeftColor  = TCS3472_Left_Read();

	        tSens = now;
	      }

	      /* 7.3) OLED: panel 7 linii (wyniesiony do oled_panel.*) */
	      if ((now - tOLED) >= PERIOD_OLED_MS) {
	        OLED_Panel_ShowSensors(&g_RightLuna, &g_LeftLuna,
	                               &g_RightColor, &g_LeftColor);
	        tOLED = now;
	      }

	      /* 7.4) UART: panel ANSI „w miejscu” (debug) */
	      if ((now - tUART) >= PERIOD_UART_MS) {
	        DebugUART_SensorsDual(&g_RightLuna, &g_LeftLuna,
	                              &g_RightColor, &g_LeftColor);
	        tUART = now;
	      }

	      /* 7.5) (Miejsce na AI Sumo – w przyszłości)
	         AI_Sumo_Update(&g_RightLuna, &g_LeftLuna, &g_RightColor, &g_LeftColor);
	         // AI ustawi docelowe prędkości przez Tank_SetTarget / Tank_*()
	       */

	    /* (Miejsce na przyszłą logikę AI Sumo – osobny moduł)
	       np.: AI_Sumo_Update(&g_RightLuna, &g_LeftLuna, &g_RightColor, &g_LeftColor);
	       która w razie potrzeby wywoła Tank_SetSpeed(left,right). */

	        /* 3.4) Miejsce na przyszły algorytm jazdy/AI (osobny moduł)
	                – w kolejnym kroku dołożymy tank_drive.c/.h i wywołania tutaj */
	        /* np. AI_Sumo_Update(&g_RightLuna, &g_LeftLuna, &g_RightColor, &g_LeftColor); */





    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
