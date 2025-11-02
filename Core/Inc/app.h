#pragma once
/*
 * ============================================================================
 *  MODULE: app – warstwa harmonogramu aplikacji
 *  Cel: wyczyścić main.c (Twoja konwencja) – tu trzymamy App_Init()/App_Tick().
 *
 *  Styl: nagłówek + krótkie komentarze przy liniach; bez nadmiaru @brief/@param.
 * ============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

void App_Init(void);   /* jednorazowa inicjalizacja modułów i pierwsze odczyty */
void App_Tick(void);   /* cykliczne zadania: Tank/Sens/OLED/UART */

#ifdef __cplusplus
}
#endif
