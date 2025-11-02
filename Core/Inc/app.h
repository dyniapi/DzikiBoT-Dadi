#pragma once
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
 *    - Nazwy funkcji/typów/zmiennych — BEZ ZMIAN (spójne z projektem).
 *    - Interwały PERIOD_* pobieramy z config.c (CFG_Scheduler()), ale makra PERIOD_* zostają.
 *    - Napęd: TIM1 CH1=PA8 (Right), CH4=PA11 (Left). ARM ESC: 3000 ms neutralu na starcie.
 * ============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Jednorazowa inicjalizacja aplikacji (wywołaj po initach Cube/HAL w main.c) */
void App_Init(void);

/* Pętla zadań cyklicznych — wołana w każdej iteracji while(1) w main.c */
void App_Tick(void);

#ifdef __cplusplus
}
#endif
